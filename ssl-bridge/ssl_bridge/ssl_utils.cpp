// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 11/18/2016
// Copyright (c) 2016-2024 DataReel Software Development
// ----------------------------------------------------------- // 
// ------------- Program Description and Details ------------- // 
// ----------------------------------------------------------- // 
/*
This file is part of the DataReel software distribution.

Datareel is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version. 

Datareel software is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with the DataReel software distribution.  If not, see
<http://www.gnu.org/licenses/>.

SSL config for DR load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include "drlb_server.h"
#include "ssl_config.h"
#include "ssl_utils.h"

#include <stdio.h>
#include <string>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/opensslv.h>

// 11/18/2026: OpenSSL 3.5 LTS or later: hybrid ML-KEM key exchange and
// ML-DSA certificates need 3.5. RHEL 9 and 10 use the system OpenSSL
// 3.5; the RHEL 8 drlb_ssl_server-openssl35 package links a static
// OpenSSL 3.5 (rpm_builder/build_openssl35.sh, OPENSSL_PREFIX and
// OPENSSL_STATIC=1 in env/linux.env).
//
// The one exception is the RHEL 8 FIPS build (FIPS_BUILD=1, the RHEL 8
// drlb_ssl_server package, as in DRCM 1.49): it links the RHEL 8 system
// OpenSSL 1.1.1, which Red Hat validated for FIPS 140, and has no
// post-quantum TLS.
#if defined(DRLB_FIPS_BUILD)
#if OPENSSL_VERSION_NUMBER < 0x10101000L
#error "The drlb_ssl_server FIPS build needs the system OpenSSL 1.1.1 or later headers."
#endif
#define DRLB_MIN_OPENSSL 0x10101000UL
#else
#if OPENSSL_VERSION_NUMBER < 0x30500000L
#error "drlb_ssl_server needs OpenSSL 3.5 or later headers. On RHEL 8 build the static OpenSSL 3.5 (rpm_builder/build_openssl35.sh, OPENSSL_PREFIX=/opt/openssl-3.5 OPENSSL_STATIC=1) or the FIPS build on the system OpenSSL (FIPS_BUILD=1)."
#endif
#define DRLB_MIN_OPENSSL 0x30500000UL
#endif

#ifndef DRLB_OPENSSL_FLAVOR
#define DRLB_OPENSSL_FLAVOR "system"
#endif

// 11/18/2026: Default key exchange groups (OpenSSL 3.5 tuple syntax). The
// first tuple holds the hybrid post-quantum ML-KEM groups, so the server
// picks one whenever the client supports it. "*" groups get a key share
// in the client hello (backend connections). "?" skips a group the
// provider does not have. The RHEL 8 and RHEL 9 crypto policies do not
// offer ML-KEM, so the bridge sets the list itself.
static const char *DefaultGroups =
  "?*X25519MLKEM768:?SecP256r1MLKEM768:?SecP384r1MLKEM1024"
  "/?*X25519:?secp256r1:?X448:?secp384r1:?secp521r1"
  "/?ffdhe2048:?ffdhe3072";

// 11/18/2026: Default signature algorithms: ML-DSA first, then the
// classical algorithms of the RHEL DEFAULT crypto policy
static const char *DefaultSigalgs =
  "?mldsa65:?mldsa87:?mldsa44"
  ":?ecdsa_secp256r1_sha256:?ecdsa_secp384r1_sha384:?ecdsa_secp521r1_sha512"
  ":?ed25519:?ed448"
  ":?rsa_pss_pss_sha256:?rsa_pss_pss_sha384:?rsa_pss_pss_sha512"
  ":?rsa_pss_rsae_sha256:?rsa_pss_rsae_sha384:?rsa_pss_rsae_sha512"
  ":?rsa_pkcs1_sha256:?rsa_pkcs1_sha384:?rsa_pkcs1_sha512";

int check_openssl_version()
// 11/18/2026: The headers are checked at build time; this catches an
// older shared library at run time (3.5, or 1.1.1 for the FIPS build).
{
  if(OpenSSL_version_num() < DRLB_MIN_OPENSSL) {
#if defined(DRLB_FIPS_BUILD)
    NT_print("ERROR - The drlb_ssl_server FIPS build needs OpenSSL 1.1.1 or later, the library is", OpenSSL_version(OPENSSL_VERSION));
#else
    NT_print("ERROR - drlb_ssl_server needs OpenSSL 3.5 or later, the library is", OpenSSL_version(OPENSSL_VERSION));
    NT_print("ERROR - On RHEL 8 install the drlb_ssl_server-openssl35 package (OpenSSL 3.5 built in)");
#endif
    return 1;
  }
  // 11/19/2026: The static OpenSSL 3.5 build (RHEL 8 drlb_ssl_server-openssl35)
  // is not a validated FIPS module: it does not run on a host in FIPS mode
  if(strcmp(DRLB_OPENSSL_FLAVOR, "static") == 0 && fips_mode()) {
    NT_print("ERROR - This host is in FIPS mode, and this program's built-in OpenSSL is not FIPS validated");
    NT_print("ERROR - On a FIPS mode host install the drlb_ssl_server package (FIPS build on the system OpenSSL):");
    NT_print("ERROR -   dnf swap drlb_ssl_server-openssl35 ./drlb_ssl_server-<version>-1.el8.x86_64.rpm");
    return 1;
  }
  return 0;
}

int fips_mode()
// 11/18/2026: 1 if the system or OpenSSL is in FIPS mode
{
  FILE *fp = fopen("/proc/sys/crypto/fips_enabled", "r");
  if(fp) {
    int c = fgetc(fp);
    fclose(fp);
    if(c == '1') return 1;
  }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  return EVP_default_properties_is_fips_enabled(0) == 1;
#else
  return FIPS_mode() != 0;
#endif
}

int pqc_build()
// 11/18/2026: 1 if this build has the post-quantum TLS algorithms
{
#if defined(DRLB_FIPS_BUILD)
  return 0;
#else
  return 1;
#endif
}

void openssl_build_info(gxString &s)
// 11/18/2026: The OpenSSL the program runs with, for --version and the log
{
  s << clear << "Linked with: " << OpenSSL_version(OPENSSL_VERSION) << ", " << DRLB_OPENSSL_FLAVOR;
#if defined(DRLB_FIPS_BUILD)
  s << ", FIPS build (system crypto policy, no post-quantum TLS)";
#else
  s << ", post-quantum TLS (ML-KEM, ML-DSA)";
#endif
  if(fips_mode()) s << ", FIPS mode on";
}

static void trim_str(std::string &s)
{
  size_t b = s.find_first_not_of(" \t\r\n"), e = s.find_last_not_of(" \t\r\n");
  s = (b == std::string::npos) ? "" : s.substr(b, e - b + 1);
}

int apply_crypto_policy(SSL_CTX *ctx, int server, const char *cert_file)
// 11/19/2026: The static OpenSSL 3.5 of the RHEL 8 drlb_ssl_server-openssl35
// package does not read /etc/pki/tls/openssl.cnf, so the bridge applies the
// RHEL system crypto policy itself: each "Name = Value" line of the OpenSSL
// back end of the active policy (ssl_crypto_policy_file, default
// /etc/crypto-policies/back-ends/opensslcnf.config) is applied with
// SSL_CONF_cmd(), as the system OpenSSL does (TLS.MinProtocol and
// TLS.MaxProtocol as MinProtocol and MaxProtocol, DTLS lines skipped). The
// bridge's TLS 1.2 minimum and ssl_protocol still apply, and the ssl_*
// settings are applied after the policy. cert_file is loaded again after
// the policy so the policy's security level checks it. Builds on the system
// OpenSSL get the policy from OpenSSL itself.
{
  if(strcmp(DRLB_OPENSSL_FLAVOR, "static") != 0) return 0;
  gxString fname = sslcfg->ssl_crypto_policy_file, up = fname;
  up.ToUpper();
  if(fname.is_null() || up == "NONE") return 0;
  FILE *fp = fopen(fname.c_str(), "r");
  if(!fp) {
    NT_print("ERROR - Cannot read the system crypto policy:", fname.c_str());
    NT_print("ERROR - Set ssl_crypto_policy_file = none to run without it");
    return 1;
  }
  int min_before = SSL_CTX_get_min_proto_version(ctx), max_before = SSL_CTX_get_max_proto_version(ctx);
  SSL_CONF_CTX *cctx = SSL_CONF_CTX_new();
  if(!cctx) { fclose(fp); return 1; }
  SSL_CONF_CTX_set_flags(cctx, SSL_CONF_FLAG_FILE | SSL_CONF_FLAG_SHOW_ERRORS |
			 (server ? SSL_CONF_FLAG_SERVER : SSL_CONF_FLAG_CLIENT));
  SSL_CONF_CTX_set_ssl_ctx(cctx, ctx);
  int errors = 0, applied = 0;
  char line[4096];
  while(fgets(line, sizeof(line), fp)) {
    std::string l(line);
    size_t c = l.find('#');
    if(c != std::string::npos) l.erase(c);
    size_t eq = l.find('=');
    if(eq == std::string::npos) continue;
    std::string name = l.substr(0, eq), value = l.substr(eq + 1);
    trim_str(name); trim_str(value);
    if(name.empty() || value.empty() || name.compare(0, 5, "DTLS.") == 0) continue;
    if(name.compare(0, 4, "TLS.") == 0) name.erase(0, 4);
    ERR_clear_error();
    int rv = SSL_CONF_cmd(cctx, name.c_str(), value.c_str());
    if(rv == 2 || rv == 1) { applied++; continue; }
    if(rv == -2) {
      NT_print("WARNING - Crypto policy setting not known to this OpenSSL, skipped:", name.c_str());
      continue;
    }
    gxString m;
    m << clear << "ERROR - Bad crypto policy setting in " << fname << ": " << name.c_str() << " = " << value.c_str();
    NT_print(m.c_str());
    errors++;
  }
  fclose(fp);
  SSL_CONF_CTX_finish(cctx);
  SSL_CONF_CTX_free(cctx);
  if(errors) return 1;
  // The policy may make the protocol range stricter, never looser
  if(SSL_CTX_get_min_proto_version(ctx) < min_before) SSL_CTX_set_min_proto_version(ctx, min_before);
  if(max_before && (SSL_CTX_get_max_proto_version(ctx) == 0 || SSL_CTX_get_max_proto_version(ctx) > max_before))
    SSL_CTX_set_max_proto_version(ctx, max_before);
  (void)cert_file; // The frontend loads its certificate after the policy
  if(server) {
    gxString m;
    m << clear << "System crypto policy applied (" << applied << " settings): " << fname;
    NT_print(m.c_str());
  }
  return 0;
}

int set_groups_sigalgs(SSL_CTX *ctx)
// 11/18/2026: Key exchange groups and signature algorithms for one
// context. Empty settings use the defaults above, except in FIPS mode
// where the crypto policy lists are kept. "system" keeps the crypto
// policy lists. The FIPS build always keeps the crypto policy lists.
{
#if defined(DRLB_FIPS_BUILD)
  (void)ctx;
  return 0;
#else
  int fips = fips_mode();
  gxString groups = sslcfg->ssl_groups, sigalgs = sslcfg->ssl_sigalgs;
  if(groups.is_null()) groups = fips ? "system" : DefaultGroups;
  if(sigalgs.is_null()) sigalgs = fips ? "system" : DefaultSigalgs;
  gxString g = groups, a = sigalgs;
  g.ToUpper(); a.ToUpper();
  if(g != "SYSTEM" && SSL_CTX_set1_groups_list(ctx, groups.c_str()) != 1) {
    NT_print("ERROR - Bad ssl_groups:", groups.c_str());
    return 1;
  }
  if(a != "SYSTEM" && SSL_CTX_set1_sigalgs_list(ctx, sigalgs.c_str()) != 1) {
    NT_print("ERROR - Bad ssl_sigalgs:", sigalgs.c_str());
    return 1;
  }
  return 0;
#endif
}

int init_ssl_cert()
{
  gxSSL *ssl = &sslcfg->openssl;
  gxString sbuf;

  if(!futils_exists(sslcfg->ssl_key_file.c_str())) {
    NT_print("ERROR - Missing private key:", sslcfg->ssl_key_file.c_str());
    return 1;
  }
  if(!futils_exists(sslcfg->ssl_cert_file.c_str())) {
    NT_print("ERROR - Missing SSL cert:", sslcfg->ssl_cert_file.c_str());
    return 1;
  }
  if(sslcfg->ssl_use_dhparms) {
    if(!futils_exists(sslcfg->ssl_dhparms_file.c_str())) {
      NT_print("ERROR - Missing dhparms file:", sslcfg->ssl_dhparms_file.c_str());
      return 1;
    }
  }

  openssl_build_info(sbuf);
  NT_print(sbuf.c_str());
  if(check_openssl_version() != 0) return 1;

  // 10/25/2026: Only TLS 1.2 and TLS 1.3. SSLv2, SSLv3, TLS 1.0, TLS 1.1
  // and DTLS are no longer accepted.
  sslcfg->ssl_protocol.ToUpper();
  if(sslcfg->ssl_protocol == "TLS") {
    sslcfg->meth = gxSSL_TLS;
    NT_print("SSL connections will use TLS 1.2 or TLS 1.3"); 
  }
  else if(sslcfg->ssl_protocol == "TLSV1_2") {
    sslcfg->meth = gxSSL_TLSv1_2;
    NT_print("SSL connections will only use TLS 1.2"); 
    if(pqc_build()) NT_print("WARNING - TLS 1.2 has no post-quantum key exchange, use ssl_protocol = TLS or TLSv1_3");
  }
  else if(sslcfg->ssl_protocol == "TLSV1_3") {
    sslcfg->meth = gxSSL_TLSv1_3;
    NT_print("SSL connections will only use TLS 1.3"); 
  }
  else {
    NT_print("ERROR - Unsupported ssl_protocol:", sslcfg->ssl_protocol.c_str());
    NT_print("ERROR - Use TLS, TLSv1_2 or TLSv1_3");
    return 1;
  }

  if(!gxSSL::InitSSLibrary()) {
    NT_print("ERROR - Cannot initializing SSL library");
    return 1;
  }

  // Frontend server context
  ssl->SetSSLProto(sslcfg->meth);
  ssl->MakeServer();
  if(!sslcfg->ssh_ca_list_file.is_null()) ssl->SetCAList(sslcfg->ssh_ca_list_file.c_str());
  NT_print("Loading SSL key and cert");
  // 11/19/2026: The static OpenSSL build applies the crypto policy before
  // the key and certificate are loaded, so the policy's security level
  // decides which certificates are accepted (LEGACY: SECLEVEL 1, FUTURE: 3)
  int policy_first = strcmp(DRLB_OPENSSL_FLAVOR, "static") == 0;
  if(policy_first) {
    if(!ssl->InitCTX("", "") || apply_crypto_policy(ssl->GetCTX(), 1, 0) != 0) {
      NT_print("ERROR - Cannot initialize the SSL server context");
      return 1;
    }
    if(SSL_CTX_use_certificate_chain_file(ssl->GetCTX(), sslcfg->ssl_cert_file.c_str()) != 1 ||
       SSL_CTX_use_PrivateKey_file(ssl->GetCTX(), sslcfg->ssl_key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
      NT_print("ERROR - Problem loading key and cert (the system crypto policy's security level also applies)");
      gxString err;
      gxSSL::GlobalSSLError(err);
      if(!err.is_null()) NT_print(err.c_str());
      return 1;
    }
  }
  else if(!ssl->InitCTX(sslcfg-> ssl_cert_file.c_str(), sslcfg->ssl_key_file.c_str())) {
    NT_print("ERROR - Problem loading key and cert");
    NT_print(ssl->SSLExceptionMessage());
    gxString err;
    gxSSL::GlobalSSLError(err);
    if(!err.is_null()) NT_print(err.c_str());
    return 1;
  }
  if(SSL_CTX_check_private_key(ssl->GetCTX()) != 1) {
    NT_print("ERROR - Private key does not match the certificate:", sslcfg->ssl_key_file.c_str());
    return 1;
  }
  if(EVP_PKEY *pk = SSL_CTX_get0_privatekey(ssl->GetCTX())) {
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    const char *type = EVP_PKEY_get0_type_name(pk);
#else
    const char *type = OBJ_nid2sn(EVP_PKEY_base_id(pk));
#endif
    NT_print("Certificate key type:", type ? type : "unknown");
  }
  if(set_ssl_ciphers(ssl->GetCTX()) != 0) return 1;
  if(set_groups_sigalgs(ssl->GetCTX()) != 0) return 1;
  if(!pqc_build()) {
    NT_print("INFO - FIPS build: post-quantum TLS (ML-KEM key exchange, ML-DSA certificates) is not available");
    NT_print("INFO - TLS key exchange groups and signature algorithms follow the system crypto policy");
    gxString g = sslcfg->ssl_groups, a = sslcfg->ssl_sigalgs;
    g.ToUpper(); a.ToUpper();
    if((!g.is_null() && g != "SYSTEM") || (!a.is_null() && a != "SYSTEM")) {
      NT_print("WARNING - ssl_groups and ssl_sigalgs are ignored in the FIPS build");
    }
  }
  else if(sslcfg->ssl_groups.is_null()) {
    NT_print("TLS key exchange groups:", fips_mode() ? "system (FIPS mode)" : "default (hybrid ML-KEM first)");
  }
  else {
    NT_print("TLS key exchange groups:", sslcfg->ssl_groups.c_str());
  }

  if(sslcfg->ssl_use_dhparms) {
    NT_print("Loading Diffie-Hellman parameters");
    if(ssl->LoadDHParms(sslcfg->ssl_dhparms_file.c_str()) != gxSSL_NO_ERROR) {
      NT_print("ERROR - Problem loading DH parms");
      NT_print(ssl->SSLExceptionMessage());
      return 1;
    }
  }
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  else {
    SSL_CTX_set_dh_auto(ssl->GetCTX(), 1); // Built in RFC 7919 groups for DHE suites
  }
#endif

  // 10/25/2026: Backend client context, created once and shared by all
  // backend connections. Certificates are verified against the CA list,
  // or the system trust store, unless ssl_verify_backend_cert = 0.
  if(sslcfg->ssl_encrypt_backend) {
    gxSSL *b = &sslcfg->backend;
    b->SetSSLProto(sslcfg->meth);
    b->MakeClient();
    if(sslcfg->ssl_verify_backend_cert) {
      b->RequireVerifyCert();
      if(!sslcfg->ssl_backend_ca_list_file.is_null()) b->SetCAList(sslcfg->ssl_backend_ca_list_file.c_str());
      NT_print("Backend certificates will be verified");
    }
    else {
      NT_print("WARNING - Backend certificates will not be verified");
    }
    if(!b->InitCTX()) {
      NT_print("ERROR - Cannot initialize the backend SSL context");
      NT_print(b->SSLExceptionMessage());
      if(!sslcfg->ssl_backend_ca_list_file.is_null()) {
	NT_print("ERROR - Check ssl_backend_ca_list_file:", sslcfg->ssl_backend_ca_list_file.c_str());
      }
      return 1;
    }
    // 11/18/2026: The static OpenSSL's default trust paths are under its
    // own prefix, so also load the RHEL system trust store
    if(sslcfg->ssl_verify_backend_cert && sslcfg->ssl_backend_ca_list_file.is_null() &&
       futils_exists("/etc/pki/tls/certs/ca-bundle.crt")) {
      SSL_CTX_load_verify_locations(b->GetCTX(), "/etc/pki/tls/certs/ca-bundle.crt", 0);
    }
    if(apply_crypto_policy(b->GetCTX(), 0, 0) != 0) return 1;
    if(set_ssl_ciphers(b->GetCTX()) != 0) return 1;
    if(set_groups_sigalgs(b->GetCTX()) != 0) return 1;
  }

  return 0;
}

int set_ssl_ciphers(SSL_CTX *ctx)
// 10/25/2026: Ciphers follow the system crypto policy unless the config
// restricts them. The old code set "ALL" on every connection.
{
  if(!sslcfg->ssl_cipher_list.is_null()) {
    if(SSL_CTX_set_cipher_list(ctx, sslcfg->ssl_cipher_list.c_str()) != 1) {
      NT_print("ERROR - Bad ssl_cipher_list:", sslcfg->ssl_cipher_list.c_str());
      return 1;
    }
  }
#if OPENSSL_VERSION_NUMBER >= 0x10101000L
  if(!sslcfg->ssl_ciphersuites.is_null()) {
    if(SSL_CTX_set_ciphersuites(ctx, sslcfg->ssl_ciphersuites.c_str()) != 1) {
      NT_print("ERROR - Bad ssl_ciphersuites:", sslcfg->ssl_ciphersuites.c_str());
      return 1;
    }
  }
#endif
  return 0;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
