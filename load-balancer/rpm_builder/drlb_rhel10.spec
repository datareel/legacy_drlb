BuildArch: x86_64
Name: drlb_server
Version: 1.62
Release: 1.el10
Summary: Datareel Load Balancer RPM
Group: System Environment/Daemons
License: GPL
Distribution: Datareel Open Source
URL: https://datareel.com/drlb
Vendor: Datareel Open Source
Requires: systemd
Requires: bash
Requires(pre): shadow-utils
Recommends: NetworkManager
Recommends: firewalld

%description
The DataReel load balancer (DRLB) is a software based TCP 
load balancing. Designed to handle any type of TCP connection, 
acting as a front end server distributing high volumes of TCP 
connections to multiple back end servers.

%install
tar xvf drlb_server.tar.gz -C $RPM_BUILD_ROOT

%files
%dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0750, drlb, drlb) "/var/log/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_server"
%config(noreplace) %attr(0644, root, root) "/etc/sysconfig/drlb_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/active_configs.list"
%config %attr(0644, root, root) "/etc/drlb/http_rr_test.cfg"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_server.8.gz"
%attr(0644, root, root) "/usr/lib/systemd/system/drlb_server.service"
%attr(0644, root, root) "/usr/lib/systemd/system/drlb_server@.service"
%dir %attr(0755, root, root) "/usr/libexec/drlb"
%attr(0755, root, root) "/usr/libexec/drlb/drlb_platform_check"
%attr(0755, root, root) "/usr/libexec/drlb/drlb_server_ctl"

%pre -p /bin/sh

# 10/25/2026: The servers run as the drlb system user
getent group drlb >/dev/null || groupadd -r drlb
getent passwd drlb >/dev/null || useradd -r -g drlb -d /var/log/drlb -s /sbin/nologin -c "DataReel load balancer" drlb
exit 0

%post -p /bin/sh

if [ $1 -eq 1 ] ; then 
    # Initial installation 
    systemctl preset drlb_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Tue Nov 17 2026 Datareel <datareel.com>
- Version 1.62, a patch release of this legacy package
- DataReel 6.4.1 library update (was 6.4 beta 1)
* Sat Oct 31 2026 Datareel <datareel.com>
- Version 1.61, the final release of this legacy package
* Mon Oct 26 2026 Datareel <datareel.com>
- Version 1.60 beta
- Security fixes from the October 2026 review, new poll() based relay
- DataReel 6.2 library update
- RHEL 9 and RHEL 10 builds; RHEL 8 only with NetworkManager and firewalld
- RHEL 6 and RHEL 7 support removed
- Native systemd units in /usr/lib/systemd/system, SysV init script removed
- Servers run as the drlb user with systemd sandboxing
* Mon Feb 05 2024 Datareel <datareel.com>
- Updates for RHEL 8 and RHEL 9 builds
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build
