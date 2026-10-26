all:
	if [ ! -f ../lib/libdatareel_ssl.a ]; then make -f makelib.mak all_install; fi
	make -f makedrlbssl.mak

clean:
	make -f makedrlbssl.mak clean
	rm -f core.*
	rm -f vgcore.*

clean_lib:
	rm -fv ../lib/libdatareel_ssl.a

install:
	cp -fv drlb_ssl_server ../bin/drlb_ssl_server
	chmod 755 ../bin/drlb_ssl_server

install_root:
	mkdir -pv /etc/drlb
	chmod 755 /etc/drlb
	getent group drlb >/dev/null || groupadd -r drlb
	getent passwd drlb >/dev/null || useradd -r -g drlb -d /var/log/drlb -s /sbin/nologin -c "DataReel load balancer" drlb
	mkdir -pv /var/log/drlb
	chown drlb:drlb /var/log/drlb
	chmod 750 /var/log/drlb
	mkdir -pv /etc/drlb/ssl/private /etc/drlb/ssl/certs
	chmod 755 /etc/drlb/ssl/certs
	chown root:drlb /etc/drlb/ssl/private
	chmod 750 /etc/drlb/ssl/private
	cp -fv drlb_ssl_server /usr/sbin/drlb_ssl_server
	chmod 755 /usr/sbin/drlb_ssl_server
	if [ ! -f /etc/drlb/ssl_bridge.cfg ]; then cp -fv ../etc/ssl_bridge.cfg  /etc/drlb/ssl_bridge.cfg; fi
	chmod 644 /etc/drlb/ssl_bridge.cfg
	if [ ! -f /etc/sysconfig/drlb_ssl_server ]; then cp -fv ../etc/drlb_ssl_server_sysconfig  /etc/sysconfig/drlb_ssl_server; fi
	chmod 644 /etc/sysconfig/drlb_ssl_server
	mkdir -pv /usr/libexec/drlb
	chmod 755 /usr/libexec/drlb
	cp -fv ../systemd_init/drlb_platform_check /usr/libexec/drlb/drlb_platform_check
	chmod 755 /usr/libexec/drlb/drlb_platform_check
	cp -fv ../systemd_init/drlb_ssl_server.service /usr/lib/systemd/system/drlb_ssl_server.service
	chmod 644 /usr/lib/systemd/system/drlb_ssl_server.service
	systemctl daemon-reload
	mkdir -p /usr/share/doc/drlb_ssl_bridge
	chmod 755 /usr/share/doc/drlb_ssl_bridge
	cp -fv ../README_drlb.txt /usr/share/doc/drlb_ssl_bridge/README.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/README.txt
	cp -fv ../README.txt /usr/share/doc/drlb_ssl_bridge/README_ssl.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/README_ssl.txt
	cp -fv ../COPYING /usr/share/doc/drlb_ssl_bridge/COPYING
	chmod 644 /usr/share/doc/drlb_ssl_bridge/COPYING
	cp -fv ../version.txt /usr/share/doc/drlb_ssl_bridge/version.txt
	chmod 644 /usr/share/doc/drlb_ssl_bridge/version.txt
	gzip -c ../man/drlb_ssl_server.8 > /usr/share/man/man8/drlb_ssl_server.8.gz
	chmod 644  /usr/share/man/man8/drlb_ssl_server.8.gz
