README file for DRLB LDM server 1.66
Last Modified: 11/17/2026

NOTE: 1.66 is a patch release (DataReel 6.4.1 library) of the
legacy DRLB LDM load balancer (drlb_ldm_server) package. drlb_server 2.0 does not replace the LDM load
balancer. Its source and releases stay in the legacy_drlb repository.

Contents:
--------
* Overview
* RPM Install
* Building and Installing
* Regression tests
* Remaining work on this project
* Support and Bug Tracking

Overview
--------
The DRLB LDM service is a front-end load balancer for the Unidata LDM
(Local Data Manager) protocol.

The DRLB LDM service uses your existing ldmd.conf (plus include files)
to allow or deny incoming client requests to LDM product feeds. The
DRLB LDM server listening on port 388 routes LDM requests and data to
any number of back-end clients. Each back-end client runs an LDM
server. The ldmd.conf ALLOW and ACCEPT rules are applied by the
front-end load balancer. If the incoming client request does not have
matching ALLOW or ACCEPT rule the front-end LB drops the client
connection. If the client request for a feed type does not match any
ALLOW rules, with optional OK and NOT patterns, the front-end LB drops
the client connection. If a client sends a product and the incoming
product does not match any ACCEPT rules the client connection is
dropped. Client connections meeting the ALLOW and/or ACCEPT criteria
are forward on to a back-end LDM server in accordance with the LB
scheme. By using the front-end LB to process initial LDM client
requests we conserve CPU, memory, and network resources on all
back-end LDM servers. This methodology also allows you to run the
latest version of LDM on back-end clients without having to delay
upgrades due to clients running old versions of LDM.

DRLB source code is distributed as an open source project under the
GNU general public license and can used by freely by commercial,
Government, and academic institutions.

RPM Install
-----------
System administrators, follow the instructions below to build an RPM
for distribution. Build each RPM on the RHEL version it is for, or in
a matching UBI container. RHEL 9 and RHEL 10 are supported. RHEL 8 is
supported only with NetworkManager and firewalld. RHEL 6 and RHEL 7
are no longer supported. The build needs these packages:

# dnf -y install gcc-c++ make rpm-build

$ cd $HOME; mkdir -pv git; cd $HOME/git
$ wget https://datareel.com/downloads/drlb/LDM-load-balancer-<version>.tar.gz
$ tar xzf LDM-load-balancer-<version>.tar.gz
$ mv LDM-load-balancer-<version> LDM-load-balancer
$ cd ${HOME}/git/LDM-load-balancer/rpm_builder

RHEL 10:
$ ./make_ldm_rpm_package.sh rhel10
$ sudo dnf -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_ldm_server-*.el10.x86_64.rpm

RHEL 9:
$ ./make_ldm_rpm_package.sh rhel9
$ sudo dnf -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_ldm_server-*.el9.x86_64.rpm

RHEL 8 (NetworkManager and firewalld only):
$ ./make_ldm_rpm_package.sh rhel8
$ sudo dnf -y install ${HOME}/rpmbuild/RPMS/x86_64/drlb_ldm_server-*.el8.x86_64.rpm

Useful RPM command to verify package contents:

List package: rpm -qf /usr/sbin/drlb_ldm_server
List files: rpm -ql drlb_ldm_server
List configs: rpm -qc drlb_ldm_server
List docs: rpm -qd drlb_ldm_server

To remove package:

$ sudo dnf -y remove drlb_ldm_server

Building and Installing
-----------------------
Follow the instructions below to build and install the DRLB LDM
server: 

$ cd $HOME
$ mkdir -pv git
$ cd $HOME/git
$ wget https://datareel.com/downloads/drlb/LDM-load-balancer-<version>.tar.gz
$ tar xzf LDM-load-balancer-<version>.tar.gz
$ mv LDM-load-balancer-<version> LDM-load-balancer
$ source $HOME/git/LDM-load-balancer/env/gnu_env.sh
$ cd $HOME/git/LDM-load-balancer/drlb_ldm
$ make
$ sudo make install_root

The DRLB LDM service must have access to your LDM configuration, the
default location is: 

/usr/local/ldm/etc/ldmd.conf

NOTE: On the front-end LDM you will not be running the LDM server. If
you want to store your ldmd.conf (plus any include files) in another
local you can modify the LB configuration to point to another
location. 

The default LB configuration file for LDM clusters is:

/etc/drlb/ldm_cluster.cfg

In the LB configuration file, refer to the following section for all
settings specific to DRLB LDM: 

[LDMCONFIG]
ldm_etc_dir = /usr/local/ldm/etc
ldmd_conf_file = ldmd.conf
resolve_ldm_hostnames = 1

For all other DRLB setting please refer to the DRLB README file:

# more /usr/share/doc/drlb_ldm/README.txt 

or online: 

http://datareel.com/drlb/

The default LB scheme used with LDM is assigned. The default
assignment rules file is: 

/etc/drlb/ldm_cluster_rules.cfg

The assignment rules allow you to route LDM requests to specific
back-end servers. This is done to isolate clients making excessive
requests or manage clients with unique requirements. By default if the
assigned back-end LDM server is down, the client request will be
forward to the next available back-end LDM server. 

After setting up your LB and LDM configuration files, start the
service:

# systemctl start drlb_ldm_server
# systemctl status drlb_ldm_server

Server messages go to the systemd journal:

# journalctl -u drlb_ldm_server

To watch the log file:

# tail -f /var/log/drlb/ldm_cluster_lb.log

To watch LB stats:

# tail -n 25 -f /var/log/drlb/ldm_cluster_lb_stats.log

To make the LB service persistent: 

# systemctl enable drlb_ldm_server

Regression tests
----------------
After building, run the regression tests from the top of the source
tree. They start local test servers on 127.0.0.1 and need only Python 3
(and the openssl command for the SSL bridge):

$ python3 tests/ldm_tests.py

The tests cover load balancing, fail over, timeouts, shutdown and the
security fixes in this release. Each prints PASS or FAIL per check and
exits non-zero if any check fails.

Remaining work on this project
------------------------------

Support and Bug Tracking
------------------------

https://datareel.com/contact.php

