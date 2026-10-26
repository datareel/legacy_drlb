BuildArch: x86_64
Name: drlb_server
Version: 1.59
Release: 1.el8
Summary: Datareel Load Balancer RPM
Group: System Environment/Daemons
License: GPL
Distribution: Datareel Open Source
URL: https://datareel.com/drlb
Vendor: Datareel Open Source
Requires: systemd
Requires: bash
# RHEL 8 is supported only with NetworkManager and firewalld
Requires: NetworkManager
Requires: firewalld
Conflicts: network-scripts

%description
The DataReel load balancer (DRLB) is a software based TCP 
load balancing. Designed to handle any type of TCP connection, 
acting as a front end server distributing high volumes of TCP 
connections to multiple back end servers.

%install
tar xvf drlb_server.tar.gz -C $RPM_BUILD_ROOT

%files
%dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0755, root, root) "/var/log/drlb"
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
* Sun Oct 25 2026 Datareel <datareel.com>
- DataReel 6.2 library update
- RHEL 9 and RHEL 10 builds; RHEL 8 only with NetworkManager and firewalld
- RHEL 6 and RHEL 7 support removed
- Native systemd units in /usr/lib/systemd/system, SysV init script removed
* Mon Feb 05 2024 Datareel <datareel.com>
- Updates for RHEL 8 and RHEL 9 builds
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for potential ABRT exit from safe_close call on closed file descriptor
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build
