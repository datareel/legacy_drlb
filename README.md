# legacy_drlb: final releases of the DataReel-based DRLB packages

This repository holds the last releases of the three DRLB packages that
were built on the DataReel library, each in its own folder with its full
git history:

| Folder | Program | Latest release | Tag |
|---|---|---|---|
| `load-balancer/` | `drlb_server`, TCP load balancer | 1.62 (not tagged yet; 1.61 tagged) | `load-balancer-1.61` |
| `ssl-bridge/` | `drlb_ssl_server`, SSL bridge | 1.62 (not tagged yet; 1.61 tagged) | `ssl-bridge-1.61` |
| `LDM-load-balancer/` | `drlb_ldm_server`, LDM load balancer | 1.66 (not tagged yet; 1.65 tagged) | `LDM-load-balancer-1.65` |

Earlier tags from the original repositories are kept with the folder name
in front, for example `load-balancer-1.60-beta`.

## What replaces them

- **load-balancer** and **ssl-bridge** are replaced by **drlb_server 2.0**
  in the `tcp_load_balancer` repository: one program, written without the
  DataReel library, that runs as a plain TCP load balancer or, with an
  `[SSLCONFIG]` section in its config, as the SSL bridge. 1.60 and 1.61
  config files work with 2.0 unchanged. No further releases of these two
  packages are planned.
- **LDM-load-balancer** is not replaced. It is maintained only here.

## Building

Each folder builds on its own, as before. See the README.txt in each
folder (RPM builds are in each folder's `rpm_builder/`).

## DataReel 6.4 update (11/15/2026, not released yet)

All three packages now bundle the DataReel 6.4 beta 1 files (datareel tag
`6.4-beta1`, the 6.3.1 library) in `include/` and `src/`, in place of 6.2:
- Only the files each package already bundled are updated, plus the ones
  the 6.4 files now need: `gxdfp64.cpp`, `gxdfp64.h`, `gxdtyp64.h`,
  `gxint64.h` and `gxuint64.h`.
- The local OpenSSL changes are kept. `include/gxssl.h` (TLS 1.3 value,
  comments) was merged with 6.4. In ssl-bridge, `src/gxssl.cpp` was merged
  by hand: the TLS 1.2 minimum, `gxSSL_TLSv1_3`, verification of the host
  name or IP address in the handshake and SNI for DNS names only are kept;
  6.4 adds the handshake timeout, SIGPIPE handling and owner-only key
  files, and its subjectAltName/IP check replaces the local one.
- ssl-bridge: DataReel 6.4 applies its own handshake timeout (10 s by
  default), so `ssl_bridge/m_thread.cpp` passes `ssl_handshake_timeout_secs`
  to `gxSSL::SetHandshakeTimeout()` as well.

Tests (RHEL 10.2, gcc 14.4.1), the same on the 6.2 and the 6.4 builds:
`tests/ldm_tests.py` 21/21, `tests/drlb_tests.py` 18/18 and
`tests/ssl_bridge_tests.py` 15/15; the same suites with ASan + UBSan builds
have no sanitizer reports. A 10 minute traffic soak of drlb_server (4
clients, 200 KB transfers, a back end stopped for 10 s every minute):
about 393,000 requests, with one failed request per back end stop (the
request in flight on the stopped back end), the same as the 6.2 build;
memory flat (8 to 9 MB), clean exit. valgrind over 50,000 requests: 0
errors.

## DataReel 6.4.1 update (11/17/2026, not tagged yet)

load-balancer 1.62, ssl-bridge 1.62 and LDM-load-balancer 1.66 bundle the
DataReel 6.4.1 files (datareel branch `6.4.1`). Every bundled file in
`include/` and `src/` is now the unmodified DataReel 6.4.1 file: the
local OpenSSL changes in `gxssl.h` and the ssl-bridge `gxssl.cpp` above
are part of DataReel 6.4 itself. 6.4.1 adds FD_CLOEXEC on sockets and
the thread fixes (RUNNING state before the thread starts, monotonic
timed waits, sleeps without a locked mutex). The servers' own accept()
calls (`m_thread.cpp`) also set FD_CLOEXEC now. None of the packages use
gxStream or gxDatagram, so the 6.4 32-byte block header does not apply.

Tests (RHEL 10.2, gcc 14.4.1, -Wall, no warnings): `tests/drlb_tests.py`
18/18, `tests/ssl_bridge_tests.py` 15/15, `tests/ldm_tests.py` 21/21, and
a smoke test of each server with Python http.server back ends; the SSL
bridge accepts TLS 1.2 and TLS 1.3 and refuses TLS 1.0 and TLS 1.1.

Still to do for a release (dougg's decision): the tags
(`load-balancer-1.62`, `ssl-bridge-1.62`, `LDM-load-balancer-1.66`), the
RPM builds and the push to aws.
