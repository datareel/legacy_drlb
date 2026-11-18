BuildArch: x86_64
Name: drlb_ssl_server
Version: 1.63
Release: 1.el9
Summary: Datareel SSL Load Balancer RPM
Group: System Environment/Daemons
License: GPL
Distribution: Datareel Open Source
URL: https://datareel.com/drlb/ssl.html
Vendor: Datareel Open Source
# 11/18/2026: OpenSSL 3.5 LTS or later (the RHEL 9 system OpenSSL).
# The symbol version names the library, not a package.
Requires: libssl.so.3(OPENSSL_3.5.0)(64bit)
Requires: libcrypto.so.3(OPENSSL_3.5.0)(64bit)
Requires: systemd
Requires: bash
Requires(pre): shadow-utils
Recommends: NetworkManager
Recommends: firewalld

%description
DataReel load balancer (DRLB) for SSL protocol.

%install
tar xvf drlb_ssl_server.tar.gz -C $RPM_BUILD_ROOT

%files
%dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0750, root, drlb) "/etc/drlb/ssl/private"
%dir %attr(0755, root, root) "/etc/drlb/ssl/certs"
%dir %attr(0750, drlb, drlb) "/var/log/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_ssl_server"
%config(noreplace) %attr(0644, root, root) "/etc/sysconfig/drlb_ssl_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ssl_bridge.cfg"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/README_ssl.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ssl_bridge/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_ssl_server.8.gz"
%attr(0644, root, root) "/usr/lib/systemd/system/drlb_ssl_server.service"
%dir %attr(0755, root, root) "/usr/libexec/drlb"
%attr(0755, root, root) "/usr/libexec/drlb/drlb_platform_check"

%pre -p /bin/sh

# 10/25/2026: The servers run as the drlb system user
getent group drlb >/dev/null || groupadd -r drlb
getent passwd drlb >/dev/null || useradd -r -g drlb -d /var/log/drlb -s /sbin/nologin -c "DataReel load balancer" drlb
exit 0

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drlb_ssl_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_ssl_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_ssl_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_ssl_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Wed Nov 18 2026 Datareel <datareel.com>
- Version 1.63, a patch release of this legacy package
- Built and run only on OpenSSL 3.5 LTS or later. The 1.62 RHEL 8 RPM
  was built on OpenSSL 1.1.1k by mistake.
- Post-quantum TLS: hybrid ML-KEM key exchange (X25519MLKEM768 first),
  ML-DSA certificates, new ssl_groups and ssl_sigalgs settings
* Tue Nov 17 2026 Datareel <datareel.com>
- Version 1.62, a patch release of this legacy package
- DataReel 6.4.1 library update (was 6.4 beta 1)
* Sat Oct 31 2026 Datareel <datareel.com>
- Version 1.61, the final release of this legacy package
* Mon Oct 26 2026 Datareel <datareel.com>
- Version 1.60 beta
- Security fixes from the October 2026 review, new poll() based relay
- OpenSSL 3.5 API update, TLS 1.2 minimum, backend certificate verification
- DataReel 6.2 library update
- RHEL 9 and RHEL 10 builds; RHEL 8 only with NetworkManager and firewalld
- RHEL 6 and RHEL 7 support removed
- Native systemd units in /usr/lib/systemd/system, SysV init script removed
- Servers run as the drlb user with systemd sandboxing
* Tue Feb 06 2024 Datareel <datareel.com>
- Updates for RHEL 8 and RHEL 9 builds
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 10 2016 Datareel <datareel.com>
- Initial RPM build
