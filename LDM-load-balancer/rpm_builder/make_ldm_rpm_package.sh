umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drlb_ldm_build_${USERNAME}"

if [ -z $1 ]; then
    echo "You must supply a OS version"
    echo "Usage: ${0} rhel8|rhel9|rhel10"
    exit 1
fi

OS=${1,,}

case "${OS}" in
    rhel8|rhel9|rhel10) ;;
    *) echo "ERROR - Unsupported OS version: ${1}. Use rhel8, rhel9, or rhel10"; exit 1 ;;
esac

if [ ! -z "${2}" ]; then BUILDDIR_tmp="${2}"; fi
 
mkdir -p ${BUILDDIR_tmp}/rpmbuild/BUILD
mkdir -p ${BUILDDIR_tmp}/rpmbuild/BUILDROOT
mkdir -p ${BUILDDIR_tmp}/rpmbuild/RPMS
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SOURCES
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SPECS
mkdir -p ${BUILDDIR_tmp}/rpmbuild/SRPMS

mkdir -p ${HOME}/rpmbuild/BUILD
mkdir -p ${HOME}/rpmbuild/BUILDROOT
mkdir -p ${HOME}/rpmbuild/RPMS
mkdir -p ${HOME}/rpmbuild/SOURCES
mkdir -p ${HOME}/rpmbuild/SPECS
mkdir -p ${HOME}/rpmbuild/SRPMS

if [ ! -d ${BUILDDIR_tmp}/rpmbuild/BUILDROOT ]; then
    echo "ERROR - Cannot create BUILDROOT: ${BUILDDIR_tmp}/rpmbuild/BUILDROOT"
    exit 1
fi

if [ ! -d ${BUILDDIR_tmp}/rpmbuild/SPECS ]; then
    echo "ERROR - Cannot create SPECS dir: ${BUILDDIR_tmp}/rpmbuild/SPECS"
    exit 1
fi

cp -fv drlb_ldm_${OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ldm.spec
Version=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ldm.spec | grep 'Version:' | awk -F: '{ print $2 }')
Version=$( echo "${Version}" | sed s/' '//g)
Release=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ldm.spec | grep 'Release:' | awk -F: '{ print $2 }')
Release=$( echo "${Release}" | sed s/' '//g)

Build="${BUILDDIR_tmp}/rpmbuild/BUILD/drlb_ldm_server"
mkdir -pv "${Build}"

if [ ! -d "${Build}" ]; then
    echo "ERROR - Cannot make Build: ${Build}"
    exit 1;
fi

source ../env/gnu_env.sh
cd ../drlb_ldm
make clean_lib
make clean
make

mkdir -p ${Build}/etc/drlb
chmod 755 ${Build}/etc/drlb
mkdir -p ${Build}/var/log/drlb
chmod 755 ${Build}/var/log/drlb
mkdir -p ${Build}/usr/sbin
chmod 755 ${Build}/usr/sbin
cp -f ../drlb_ldm/drlb_ldm_server ${Build}/usr/sbin/drlb_ldm_server
chmod 755 ${Build}/usr/sbin/drlb_ldm_server
cp -f ../etc/ldm_cluster.cfg  ${Build}/etc/drlb/ldm_cluster.cfg
chmod 644 ${Build}/etc/drlb/ldm_cluster.cfg
cp -f ../etc/ldm_cluster_rules.cfg ${Build}/etc/drlb/ldm_cluster_rules.cfg
chmod 644 ${Build}/etc/drlb/ldm_cluster_rules.cfg
mkdir -p ${Build}/etc/sysconfig
chmod 755 ${Build}/etc/sysconfig
cp -f ../etc/drlb_ldm_server_sysconfig  ${Build}/etc/sysconfig/drlb_ldm_server
chmod 644 ${Build}/etc/sysconfig/drlb_ldm_server
mkdir -p ${Build}/usr/libexec/drlb
chmod 755 ${Build}/usr/libexec/drlb
cp -fv ../systemd_init/drlb_platform_check ${Build}/usr/libexec/drlb/drlb_platform_check
chmod 755 ${Build}/usr/libexec/drlb/drlb_platform_check
mkdir -p ${Build}/usr/share/doc/drlb_ldm
chmod 755 ${Build}/usr/share/doc/drlb_ldm
cp -f ../README_drlb.txt ${Build}/usr/share/doc/drlb_ldm/README.txt
chmod 644 ${Build}/usr/share/doc/drlb_ldm/README.txt
cp -f ../README.txt ${Build}/usr/share/doc/drlb_ldm/README_ldm.txt
chmod 644 ${Build}/usr/share/doc/drlb_ldm/README_ldm.txt
cp -f ../COPYING ${Build}/usr/share/doc/drlb_ldm/COPYING
chmod 644 ${Build}/usr/share/doc/drlb_ldm/COPYING
cp -f ../version.txt ${Build}/usr/share/doc/drlb_ldm/version.txt
chmod 644 ${Build}/usr/share/doc/drlb_ldm/version.txt
mkdir -p ${Build}/usr/share/man/man8
chmod 755 ${Build}/usr/share/man/man8
gzip -c ../man/drlb_ldm_server.8 > ${Build}/usr/share/man/man8/drlb_ldm_server.8.gz
chmod 644  ${Build}/usr/share/man/man8/drlb_ldm_server.8.gz
mkdir -p ${Build}/usr/lib/systemd/system
chmod 755 ${Build}/usr/lib/systemd/system
cp -fv ../systemd_init/drlb_ldm_server.service ${Build}/usr/lib/systemd/system/drlb_ldm_server.service
chmod 644 ${Build}/usr/lib/systemd/system/drlb_ldm_server.service

cd ${Build}
if [ -f ${HOME}/rpmbuild/BUILD/drlb_ldm_server.tar.gz ]; then
    rm -f ${HOME}/rpmbuild/BUILD/drlb_ldm_server.tar.gz
fi
tar cfz ${HOME}/rpmbuild/BUILD/drlb_ldm_server.tar.gz *

cd ${BUILDDIR_tmp}/rpmbuild/SPECS
# 11/18/2026: The RPM header records the build host. Use a neutral name
# so the name of the machine that built the release is not in the package.
DRLB_BUILDHOST=${DRLB_BUILDHOST:-datareel-build}
rpmbuild -ba --define "_buildhost ${DRLB_BUILDHOST}" --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drlb_ldm.spec

# 11/18/2026: The LDM load balancer uses no OpenSSL, and every package must
# record the neutral build host
RPM=${HOME}/rpmbuild/RPMS/x86_64/drlb_ldm_server-${Version}-${Release}.x86_64.rpm
if rpm -qpR "${RPM}" | grep -qE '^(libssl|libcrypto)'; then
    echo "ERROR - ${RPM} requires OpenSSL"; exit 1
fi
for r in "${RPM}" ${HOME}/rpmbuild/SRPMS/drlb_ldm_server-${Version}-${Release}.src.rpm; do
    if [ "$(rpm -qp --qf '%{BUILDHOST}' "${r}")" != "${DRLB_BUILDHOST}" ]; then
        echo "ERROR - ${r} build host is not ${DRLB_BUILDHOST}"
        exit 1
    fi
done
echo "Built ${RPM##*/} (build host ${DRLB_BUILDHOST})"

rm -f ${HOME}/rpmbuild/BUILD/drlb_ldm_server.tar.gz
rm -rf ${BUILDDIR_tmp}

