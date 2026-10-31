BuildArch: x86_64
Name: drlb_ldm_server
Version: 1.64
Release: 0.1.beta.el9
Summary: Datareel LDM Load Balancer RPM
Group: System Environment/Daemons
License: GPL
Distribution: Datareel Open Source
URL: https://datareel.com/drlb/ldm.html
Vendor: Datareel Open Source
Requires: systemd
Requires: bash
Requires(pre): shadow-utils
Recommends: NetworkManager
Recommends: firewalld

%description
DataReel load balancer (DRLB) for LDM protocol.

%install
tar xvf drlb_ldm_server.tar.gz -C $RPM_BUILD_ROOT

%files
%dir %attr(0755, root, root) "/etc/drlb"
%dir %attr(0750, drlb, drlb) "/var/log/drlb"
%attr(0755, root, root) "/usr/sbin/drlb_ldm_server"
%config(noreplace) %attr(0644, root, root) "/etc/sysconfig/drlb_ldm_server"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ldm_cluster.cfg"
%config(noreplace) %attr(0644, root, root) "/etc/drlb/ldm_cluster_rules.cfg"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/README.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/README_ldm.txt"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/COPYING"
%doc %attr(0644, root, root) "/usr/share/doc/drlb_ldm/version.txt"
%doc %attr(0644, root, root) "/usr/share/man/man8/drlb_ldm_server.8.gz"
%attr(0644, root, root) "/usr/lib/systemd/system/drlb_ldm_server.service"
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
    systemctl preset drlb_ldm_server.service >/dev/null 2>&1 || : 
fi

%preun -p /bin/sh

if [ $1 -eq 0 ] ; then 
    # Package removal, not upgrade 
    systemctl --no-reload disable drlb_ldm_server.service > /dev/null 2>&1 || : 
    systemctl stop drlb_ldm_server.service > /dev/null 2>&1 || : 
fi

%postun -p /bin/sh

systemctl daemon-reload >/dev/null 2>&1 || : 
if [ $1 -ge 1 ] ; then 
        # Package upgrade, not uninstall 
        systemctl try-restart drlb_ldm_server.service >/dev/null 2>&1 || : 
fi

%changelog
* Mon Oct 26 2026 Datareel <datareel.com>
- Version 1.64 beta
- Security fixes from the October 2026 review, new poll() based relay
- LDM RPC filter checks every FEEDME, NOTIFYME, HIYA and HEREIS
- DataReel 6.2 library update
- RHEL 9 and RHEL 10 builds; RHEL 8 only with NetworkManager and firewalld
- RHEL 6 and RHEL 7 support removed
- Native systemd units in /usr/lib/systemd/system, SysV init script removed
- Servers run as the drlb user with systemd sandboxing
* Mon Feb 05 2024 Datareel <datareel.com>
- Updates for RHEL 8 and RHEL 9 builds
* Tue Oct 24 2016 Datareel <datareel.com>
- Fix for joined feed types
* Mon Oct 24 2016 Datareel <datareel.com>
- Fix to reduce zero byte disconnects
- Thread Connection count fix
* Thu Oct 20 2016 Datareel <datareel.com>
- Fix for ldmsend file pattern name read
- Backward compatibility for LDM 6.6.5 clients
* Wed Oct 19 2016 Datareel <datareel.com>
- Fix for ABRT exit from safe_close call on closed file descriptor 
* Wed Oct 12 2016 Datareel <datareel.com>
- Fix for notifyme closing connection before completing
* Sun Oct 02 2016 Datareel <datareel.com>
- Initial RPM build
