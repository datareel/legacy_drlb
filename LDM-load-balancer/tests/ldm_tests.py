#!/usr/bin/env python3
# DRLB LDM load balancer regression tests
#
# Starts a fake back end LDM server that counts the bytes it receives,
# and drlb_ldm_server with a test ldmd.conf. Sends LDM 6 RPC messages
# (ONC RPC record marking and XDR) and checks which are passed to the
# back end and which close the connection.
#
# Usage: python3 tests/ldm_tests.py [path to drlb_ldm_server]
# Needs only Python 3 and the server binary. Uses ports 18200-18299.

import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', 'drlb_ldm', 'drlb_ldm_server'))
LB_PORT = 18200
BACKEND_PORT = 18211
WORK = tempfile.mkdtemp(prefix='drlb_ldm_tests_')
results = []

IDS, DDPLUS, NEXRAD2 = 0x8, 0x3, 0x10000000
FEEDME, HIYA, HEREIS, NOTIFYME, COMINGSOON, BLKDATA = 4, 5, 1, 9, 12, 13

LDMD_CONF = r'''# Test ldmd.conf
ALLOW IDS|DDPLUS ^127\.0\.0\.1$
ALLOW NEXRAD2 ^10\.9\.9\.9$
ACCEPT IDS ^SAUS ^127\.0\.0\.1$
'''


def u32(v):
    return struct.pack('>I', v & 0xffffffff)


def xstr(s):
    b = s.encode()
    return u32(len(b)) + b + b'\0' * ((4 - len(b) % 4) % 4)


def call(proc, args, vers=6, xid=1):
    body = u32(xid) + u32(0) + u32(2) + u32(300029) + u32(vers) + u32(proc) + u32(0) * 4 + args
    return u32(0x80000000 | len(body)) + body


def prod_class(specs):
    b = u32(0) * 4 + u32(len(specs))
    for ft, pat in specs:
        b += u32(ft) + xstr(pat)
    return b


def prod_info(ft, ident, data=b'hello\0\0\0'):
    return u32(0) * 2 + b'\x11' * 16 + xstr('origin.test') + u32(ft) + u32(1) + xstr(ident) + u32(len(data)) + data


def feedme(specs):
    return call(FEEDME, u32(1) + prod_class(specs) + u32(16384))


# name: (messages, expected result: 'open' or 'closed')
CASES = [
    ('FEEDME for an allowed feed', [feedme([(IDS, '.*')])], 'open'),
    ('FEEDME for two allowed feeds', [feedme([(IDS | DDPLUS, '.*')])], 'open'),
    ('FEEDME for LDM 5', [call(FEEDME, prod_class([(IDS, '.*')]), vers=5)], 'open'),
    ('FEEDME adding a feed that is not allowed', [feedme([(IDS | NEXRAD2, '.*')])], 'closed'),
    ('FEEDME with a second spec that is not allowed', [feedme([(IDS, '.*'), (NEXRAD2, '.*')])], 'closed'),
    ('NOTIFYME for a feed that is not allowed', [call(NOTIFYME, prod_class([(NEXRAD2, '.*')]))], 'closed'),
    ('HEREIS without HIYA', [call(HEREIS, prod_info(IDS, 'SAUS12 KXXX'))], 'closed'),
    ('HIYA then HEREIS matching ACCEPT', [call(HIYA, prod_class([(IDS, '.*')])),
                                         call(HEREIS, prod_info(IDS, 'SAUS12 KXXX'))], 'open'),
    ('HEREIS not matching prodIdEre after HIYA', [call(HIYA, prod_class([(IDS, '.*')])),
                                                 call(HEREIS, prod_info(IDS, 'SAUS12 KXXX')),
                                                 call(HEREIS, prod_info(IDS, 'FOOBAR'))], 'closed'),
    ('HEREIS for a feed not accepted', [call(HIYA, prod_class([(IDS, '.*')])),
                                       call(HEREIS, prod_info(DDPLUS, 'SAUS12 KXXX'))], 'closed'),
    ('HIYA for a feed not accepted', [call(HIYA, prod_class([(NEXRAD2, '.*')]))], 'closed'),
    ('BLKDATA without COMINGSOON', [call(HIYA, prod_class([(IDS, '.*')])),
                                   call(BLKDATA, u32(1) + b'\x11' * 16 + u32(0) + u32(4) + b'data')], 'closed'),
    ('COMINGSOON then BLKDATA', [call(HIYA, prod_class([(IDS, '.*')])),
                                call(COMINGSOON, u32(1) + prod_info(IDS, 'SAUS12 KXXX', b'') + u32(16384)),
                                call(BLKDATA, u32(1) + b'\x11' * 16 + u32(0) + u32(4) + b'data')], 'open'),
    ('second FEEDME on one connection', [feedme([(IDS, '.*')]), feedme([(IDS, '.*')])], 'closed'),
    ('RPC reply before any call', [u32(0x80000000 | 8) + u32(1) + u32(1)], 'closed'),
    ('garbage with 0xFF length bytes', [u32(0x80000000 | 200) + b'\xff' * 200], 'closed'),
    ('pattern length larger than the message', [call(FEEDME, u32(1) + u32(0) * 4 + u32(1) + u32(IDS) +
                                                     u32(0xfffffff0) + b'abcd')], 'closed'),
    ('not an LDM program', [call(FEEDME, u32(1) + prod_class([(IDS, '.*')]))[:16] + u32(100000) +
                            call(FEEDME, u32(1) + prod_class([(IDS, '.*')]))[20:]], 'closed'),
    ('FEEDME sent one byte at a time', 'BYTEWISE', 'open'),
]

received = []
received_lock = threading.Lock()


def backend_server():
    srv = socket.socket()
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(('127.0.0.1', BACKEND_PORT))
    srv.listen(64)

    def handle(c):
        n = 0
        while True:
            d = c.recv(65536)
            if not d:
                break
            n += len(d)
        with received_lock:
            received.append(n)
    while True:
        c, _ = srv.accept()
        threading.Thread(target=handle, args=(c,), daemon=True).start()


def run_case(messages):
    s = socket.create_connection(('127.0.0.1', LB_PORT))
    if messages == 'BYTEWISE':
        for b in feedme([(IDS, '.*')]):
            s.send(bytes([b]))
            time.sleep(0.001)
    else:
        for m in messages:
            try:
                s.sendall(m)
            except OSError:
                break
            time.sleep(0.05)
    time.sleep(0.3)
    s.settimeout(1)
    try:
        closed = s.recv(10) == b''
    except socket.timeout:
        closed = False
    except OSError:
        closed = True
    s.close()
    return 'closed' if closed else 'open'


def check(name, ok, detail=''):
    results.append((name, ok, detail))
    print('%-4s %s %s' % ('PASS' if ok else 'FAIL', name, detail))


def main():
    if not os.access(SERVER, os.X_OK):
        print('Server binary not found: %s' % SERVER)
        return 2
    os.makedirs(os.path.join(WORK, 'etc'))
    with open(os.path.join(WORK, 'etc', 'ldmd.conf'), 'w') as f:
        f.write(LDMD_CONF)
    cfg = os.path.join(WORK, 'ldm.cfg')
    with open(cfg, 'w') as f:
        f.write('\n'.join(['[LDMCONFIG]', 'ldm_etc_dir = %s/etc' % WORK, 'ldmd_conf_file = ldmd.conf',
                           'resolve_ldm_hostnames = 0', '[LBSERVER]', 'port = %d' % LB_PORT,
                           'enable_logging = 1', 'log_level = 1', 'logfile_name = %s/ldm.log' % WORK,
                           '[LBNODE]', 'node_name = ldm1', 'hostname = 127.0.0.1',
                           'port_number = %d' % BACKEND_PORT]) + '\n')
    threading.Thread(target=backend_server, daemon=True).start()
    out = open(os.path.join(WORK, 'ldm.out'), 'w')
    proc = subprocess.Popen([SERVER, '--config-file=' + cfg], stdout=out, stderr=subprocess.STDOUT)
    for _ in range(50):
        try:
            socket.create_connection(('127.0.0.1', LB_PORT), timeout=0.2).close()
            break
        except OSError:
            time.sleep(0.1)
    time.sleep(0.5)
    with received_lock:
        received.clear()
    for name, messages, expect in CASES:
        got = run_case(messages)
        check(name + ' -> ' + expect, got == expect, 'got ' + got)
    check('server still running after all cases', proc.poll() is None)
    proc.send_signal(signal.SIGTERM)
    rc = proc.wait(30)
    out.close()
    check('server exits cleanly', rc == 0, 'exit %s' % rc)
    failed = [r for r in results if not r[1]]
    print('\n%d passed, %d failed. Logs: %s' % (len(results) - len(failed), len(failed), WORK))
    if not failed:
        shutil.rmtree(WORK, ignore_errors=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
