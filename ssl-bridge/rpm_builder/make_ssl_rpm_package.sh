umask 0022

if [ -z $USERNAME ]; then USERNAME=$(whoami); fi
BUILDDIR_tmp="/tmp/drlb_ssl_build_${USERNAME}"

if [ -z $1 ]; then
    echo "You must supply a OS version"
    echo "Usage: ${0} rhel8|rhel8-openssl35|rhel9|rhel10"
    exit 1
fi

OS=${1,,}

# 11/18/2026: OpenSSL per package (the RHEL 8 pair follows DRCM 1.49):
#   rhel8            drlb_ssl_server: RHEL 8 system OpenSSL 1.1.1 (FIPS 140
#                    validated by Red Hat), FIPS build, classic TLS
#   rhel8-openssl35  drlb_ssl_server-openssl35: static OpenSSL 3.5
#                    (build_openssl35.sh first), post-quantum TLS. It
#                    provides and conflicts with drlb_ssl_server; switch with
#                    "dnf swap drlb_ssl_server drlb_ssl_server-openssl35"
#   rhel9, rhel10    drlb_ssl_server: system OpenSSL 3.5, post-quantum TLS
FLAVOR=system35
NAME=drlb_ssl_server
SPEC_OS=${OS}
case "${OS}" in
    rhel8) FLAVOR=fips ;;
    rhel8-openssl35) FLAVOR=static35; NAME=drlb_ssl_server-openssl35; SPEC_OS=rhel8
        OPENSSL35_PREFIX=${OPENSSL35_PREFIX:-/opt/openssl-3.5}
        if [ ! -f "${OPENSSL35_PREFIX}/lib64/libssl.a" ]; then
            echo "ERROR - No static OpenSSL 3.5 in ${OPENSSL35_PREFIX}; run build_openssl35.sh first"
            exit 1
        fi ;;
    rhel9|rhel10) ;;
    *) echo "ERROR - Unsupported OS version: ${1}. Use rhel8, rhel8-openssl35, rhel9, or rhel10"; exit 1 ;;
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

cp -fv drlb_ssl_${SPEC_OS}.spec ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec
if [ "${FLAVOR}" == "static35" ]; then
    # Provides/Conflicts go after Release:, where %{version} and %{release}
    # are defined; no OpenSSL package is required (OpenSSL is built in)
    sed -i -e 's/^Name: drlb_ssl_server$/Name: drlb_ssl_server-openssl35/' \
           -e 's/^\(Release: .*\)$/\1\nProvides: drlb_ssl_server = %{version}-%{release}\nConflicts: drlb_ssl_server/' \
           -e 's/^Summary: .*/Summary: Datareel SSL Load Balancer RPM, OpenSSL 3.5 static build (post-quantum TLS)/' \
           -e '/^Requires: openssl-libs$/d' \
           ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec
    grep -q '^Name: drlb_ssl_server-openssl35' ${BUILDDIR_tmp}/rpmbuild/SPECS/drlb_ssl.spec || { echo "ERROR - spec Name not changed"; exit 1; }
fi
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
# 11/18/2026: OpenSSL 3.5 or later, except the RHEL 8 FIPS build (1.1.1);
# ssl_utils.cpp also stops older builds
unset FIPS_BUILD OPENSSL_PREFIX OPENSSL_STATIC
MIN=0x30500000L
case "${FLAVOR}" in
    fips) export FIPS_BUILD=1; MIN=0x10101000L ;;
    static35) export OPENSSL_PREFIX=${OPENSSL35_PREFIX} OPENSSL_STATIC=1 ;;
esac
OPENSSL_INC=""
if [ -n "${OPENSSL_PKG}" ]; then OPENSSL_INC=$(pkg-config --cflags ${OPENSSL_PKG}); fi
if [ -n "${OPENSSL_PREFIX}" ]; then OPENSSL_INC="-I${OPENSSL_PREFIX}/include"; fi
OPENSSL_HDR=$(printf "#include <openssl/opensslv.h>\n#if OPENSSL_VERSION_NUMBER < ${MIN}\n#error old\n#endif\nOPENSSL_VERSION_TEXT\n" | g++ ${OPENSSL_INC} -E -P -x c++ - 2>/dev/null | tail -1)
if [ -z "${OPENSSL_HDR}" ]; then
    echo "ERROR - The OpenSSL headers are too old for ${OS}."
    echo "ERROR - RHEL 8: use rhel8 (FIPS build, system OpenSSL) or rhel8-openssl35."
    exit 1
fi
echo "Building ${NAME} (${FLAVOR}) with ${OPENSSL_HDR}"
make
LINKED=$(./drlb_ssl_server --version | grep '^Linked with:')
echo "drlb_ssl_server ${LINKED}"
case "${FLAVOR}" in
    fips)
        readelf -d drlb_ssl_server | grep -q 'NEEDED.*libssl.so.1.1' || { echo "ERROR - the rhel8 package must link the system OpenSSL 1.1.1"; exit 1; }
        echo "${LINKED}" | grep -q 'FIPS build' || { echo "ERROR - not a FIPS build"; exit 1; } ;;
    static35)
        if readelf -d drlb_ssl_server | grep -q 'NEEDED.*lib\(ssl\|crypto\)'; then
            echo "ERROR - the openssl35 package must not link a shared libssl/libcrypto"; exit 1
        fi
        readelf -d drlb_ssl_server | grep -qE 'RPATH|RUNPATH' && { echo "ERROR - the openssl35 package must have no rpath"; exit 1; }
        echo "${LINKED}" | grep -q 'OpenSSL 3\.5\..*, static, ' || { echo "ERROR - not a static OpenSSL 3.5 build"; exit 1; } ;;
    *)
        if readelf -d drlb_ssl_server | grep -q 'libssl.so.1'; then
            echo "ERROR - drlb_ssl_server links an OpenSSL older than 3: $(ldd drlb_ssl_server | grep libssl)"
            exit 1
        fi ;;
esac

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
# 11/18/2026: The RPM header records the build host. Use a neutral name
# so the name of the machine that built the release is not in the package.
DRLB_BUILDHOST=${DRLB_BUILDHOST:-datareel-build}
rpmbuild -ba --define "_buildhost ${DRLB_BUILDHOST}" --buildroot=${BUILDDIR_tmp}/rpmbuild/BUILDROOT drlb_ssl.spec

# 11/18/2026: The package's OpenSSL requirements must match its flavor
RPM=$(ls ${HOME}/rpmbuild/RPMS/x86_64/${NAME}-${Version}-${Release}.x86_64.rpm)
REQ=$(rpm -qpR "${RPM}")
case "${FLAVOR}" in
    fips) echo "${REQ}" | grep -q 'libssl.so.1.1' || { echo "ERROR - ${RPM} does not require the system OpenSSL 1.1.1"; exit 1; } ;;
    static35) if echo "${REQ}" | grep -qE 'libssl|libcrypto|openssl'; then
                  echo "ERROR - ${RPM} requires a shared OpenSSL:"; echo "${REQ}" | grep -E 'ssl|crypto'; exit 1
              fi ;;
    *) if echo "${REQ}" | grep -q 'libssl.so.1' || ! echo "${REQ}" | grep -q 'libssl.so.3(OPENSSL_3.5.0)'; then
           echo "ERROR - ${RPM} does not require OpenSSL 3.5:"; echo "${REQ}" | grep -E 'ssl|crypto'; exit 1
       fi ;;
esac
for r in "${RPM}" ${HOME}/rpmbuild/SRPMS/${NAME}-${Version}-${Release}.src.rpm; do
    if [ "$(rpm -qp --qf '%{BUILDHOST}' "${r}")" != "${DRLB_BUILDHOST}" ]; then
        echo "ERROR - ${r} build host is not ${DRLB_BUILDHOST}"
        exit 1
    fi
done
echo "OpenSSL in ${RPM##*/}: ${OPENSSL_HDR}, ${FLAVOR}, ${LINKED}"

rm -f ${HOME}/rpmbuild/BUILD/drlb_ssl_server.tar.gz
rm -rf ${BUILDDIR_tmp}

