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

  NT_print("OpenSSL version:", OpenSSL_version(OPENSSL_VERSION));

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
  if(!ssl->InitCTX(sslcfg-> ssl_cert_file.c_str(), sslcfg->ssl_key_file.c_str())) {
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
  if(set_ssl_ciphers(ssl->GetCTX()) != 0) return 1;

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
    if(set_ssl_ciphers(b->GetCTX()) != 0) return 1;
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
