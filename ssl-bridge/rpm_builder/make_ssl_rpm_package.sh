umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drlb_ssl_build_${USERNAME}"

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

cp -fv drlb_ssl_${OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec
Version=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec | grep 'Version:' | awk -F: '{ print $2 }')
Version=$( echo "${Version}" | sed s/' '//g)
Release=$(head -15 ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec | grep 'Release:' | awk -F: '{ print $2 }')
Release=$( echo "${Release}" | sed s/' '//g)

Build="${BUILDDIR_tmp}/rpmbuild/BUILD/drlb_ssl_server"
mkdir -pv "${Build}"

if [ ! -d "${Build}" ]; then
    echo "ERROR - Cannot make Build: ${Build}"
    exit 1;
fi

source ../env/gnu_env.sh
cd ../ssl_bridge
make clean_lib
make clean
# 11/18/2026: OpenSSL 3.5 or later only (ssl_utils.cpp also stops older builds)
OPENSSL_INC=""
if [ -n "${OPENSSL_PREFIX}" ]; then OPENSSL_INC="-I${OPENSSL_PREFIX}/include"; fi
OPENSSL_HDR=$(printf '#include <openssl/opensslv.h>\n#if OPENSSL_VERSION_NUMBER < 0x30500000L\n#error old\n#endif\nOPENSSL_VERSION_TEXT\n' | g++ ${OPENSSL_INC} -E -P -x c++ - 2>/dev/null | tail -1)
if [ -z "${OPENSSL_HDR}" ]; then
    echo "ERROR - OpenSSL 3.5 or later headers are required (openssl-devel)."
    echo "ERROR - On RHEL 8 apply the RHEL 8.10 updates that bring OpenSSL 3.5."
    exit 1
fi
echo "Building with ${OPENSSL_HDR}"
make
if ldd drlb_ssl_server | grep -q 'libssl.so.1'; then
    echo "ERROR - drlb_ssl_server links an OpenSSL older than 3: $(ldd drlb_ssl_server | grep libssl)"
    exit 1
fi

mkdir -p ${Build}/etc/drlb
chmod 755 ${Build}/etc/drlb
mkdir -p ${Build}/var/log/drlb
chmod 755 ${Build}/var/log/drlb
mkdir -p ${Build}/etc/drlb/ssl/private 
chmod 750 ${Build}/etc/drlb/ssl/private
mkdir -p ${Build}/etc/drlb/ssl/certs
chmod -R 755 ${Build}/etc/drlb/ssl/certs
mkdir -p ${Build}/usr/sbin
chmod 755 ${Build}/usr/sbin
cp -f ../ssl_bridge/drlb_ssl_server ${Build}/usr/sbin/drlb_ssl_server
chmod 755 ${Build}/usr/sbin/drlb_ssl_server
cp -f ../etc/ssl_bridge.cfg  ${Build}/etc/drlb/ssl_bridge.cfg
chmod 644 ${Build}/etc/drlb/ssl_bridge.cfg
mkdir -p ${Build}/etc/sysconfig
chmod 755 ${Build}/etc/sysconfig
cp -f ../etc/drlb_ssl_server_sysconfig  ${Build}/etc/sysconfig/drlb_ssl_server
chmod 644 ${Build}/etc/sysconfig/drlb_ssl_server
mkdir -p ${Build}/usr/libexec/drlb
chmod 755 ${Build}/usr/libexec/drlb
cp -fv ../systemd_init/drlb_platform_check ${Build}/usr/libexec/drlb/drlb_platform_check
chmod 755 ${Build}/usr/libexec/drlb/drlb_platform_check
mkdir -p ${Build}/usr/share/doc/drlb_ssl_bridge
chmod 755 ${Build}/usr/share/doc/drlb_ssl_bridge
cp -f ../README_drlb.txt ${Build}/usr/share/doc/drlb_ssl_bridge/README.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/README.txt
cp -f ../README.txt ${Build}/usr/share/doc/drlb_ssl_bridge/README_ssl.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/README_ssl.txt
cp -f ../COPYING ${Build}/usr/share/doc/drlb_ssl_bridge/COPYING
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/COPYING
cp -f ../version.txt ${Build}/usr/share/doc/drlb_ssl_bridge/version.txt
chmod 644 ${Build}/usr/share/doc/drlb_ssl_bridge/version.txt
mkdir -p ${Build}/usr/share/man/man8
chmod 755 ${Build}/usr/share/man/man8
gzip -c ../man/drlb_ssl_server.8 > ${Build}/usr/share/man/man8/drlb_ssl_server.8.gz
chmod 644  ${Build}/usr/share/man/man8/drlb_ssl_server.8.gz
mkdir -p ${Build}/usr/lib/systemd/system
chmod 755 ${Build}/usr/lib/systemd/system
cp -fv ../systemd_init/drlb_ssl_server.service ${Build}/usr/lib/systemd/system/drlb_ssl_server.service
chmod 644 ${Build}/usr/lib/systemd/system/drlb_ssl_server.service

cd ${Build}
if [ -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz ]; then
    rm -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz
fi
tar cfz ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz *

cd ${BUILDDIR_tmp}/rpmbuild/SPECS
rpmbuild -ba --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drlb_ssl.spec

# 11/18/2026: The package must require OpenSSL 3.5 libraries and nothing older
RPM=$(ls ${HOME}/rpmbuild/RPMS/x86_64/drlb_ssl_server-${Version}-${Release}.x86_64.rpm)
if rpm -qpR "${RPM}" | grep -q 'libssl.so.1' || ! rpm -qpR "${RPM}" | grep -q 'libssl.so.3(OPENSSL_3.5.0)'; then
    echo "ERROR - ${RPM} does not require OpenSSL 3.5:"
    rpm -qpR "${RPM}" | grep -E 'ssl|crypto'
    exit 1
fi
echo "OpenSSL in ${RPM##*/}: ${OPENSSL_HDR}"

rm -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz
rm -rf ${BUILDDIR_tmp}

