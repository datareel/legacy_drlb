all:
	if [ ! -f ../lib/libdatareel.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlb.mak

clean:
	make -f makedrlb.mak clean
	rm -f core.*
	rm -f vgcore.*

clean_lib:
	rm -fv ../lib/libdatareel.a

install:
	cp -fv drlb_server ../bin/drlb_server
	chmod 755 ../bin/drlb_server

install_root:
	mkdir -pv /etc/drlb
	chmod 755 /etc/drlb
	mkdir -pv /var/log/drlb
	chmod 755 /var/log/drlb
	cp -fv drlb_server /usr/sbin/drlb_server
	chmod 755 /usr/sbin/drlb_server
	if [ ! -f /etc/sysconfig/drlb_server ]; then cp -fv ../etc/drlb_server_sysconfig  /etc/sysconfig/drlb_server; fi
	chmod 644 /etc/sysconfig/drlb_server
	if [ ! -f /etc/drlb/active_configs.list ]; then cp -fv ../etc/active_configs.list_template /etc/drlb/active_configs.list; fi
	chmod 644 /etc/drlb/active_configs.list
	if [ ! -f /etc/drlb/http_rr_test.cfg ]; then cp -fv ../etc/http_rr_test.cfg /etc/drlb/http_rr_test.cfg; fi
	chmod 644 /etc/drlb/http_rr_test.cfg
	mkdir -pv /usr/libexec/drlb
	chmod 755 /usr/libexec/drlb
	cp -fv ../systemd_init/drlb_platform_check /usr/libexec/drlb/drlb_platform_check
	chmod 755 /usr/libexec/drlb/drlb_platform_check
	cp -fv ../systemd_init/drlb_server_ctl /usr/libexec/drlb/drlb_server_ctl
	chmod 755 /usr/libexec/drlb/drlb_server_ctl
	cp -fv ../systemd_init/drlb_server.service /usr/lib/systemd/system/drlb_server.service
	chmod 644 /usr/lib/systemd/system/drlb_server.service
	cp -fv ../systemd_init/drlb_server@.service /usr/lib/systemd/system/drlb_server@.service
	chmod 644 /usr/lib/systemd/system/drlb_server@.service
	systemctl daemon-reload
	mkdir -p /usr/share/doc/drlb
	chmod 755 /usr/share/doc/drlb
	cp -fv ../README.txt /usr/share/doc/drlb/README.txt
	chmod 644 /usr/share/doc/drlb/README.txt
	cp -fv ../COPYING /usr/share/doc/drlb/COPYING
	chmod 644 /usr/share/doc/drlb/COPYING
	cp -fv ../version.txt /usr/share/doc/drlb/version.txt
	chmod 644 /usr/share/doc/drlb/version.txt
	gzip -c ../man/drlb_server.8 > /usr/share/man/man8/drlb_server.8.gz
	chmod 644  /usr/share/man/man8/drlb_server.8.gz

