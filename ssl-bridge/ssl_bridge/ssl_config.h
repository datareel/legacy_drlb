// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/18/2016
// Copyright (c) 2016-2024 DataReel Software Development
// ----------------------------------------------------------- // 
// ---------- Include File Description and Details  ---------- // 
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
#ifndef __SSH_CONFIG_HPP__
#define __SSH_CONFIG_HPP__

#include "gxdlcode.h"
#include "gxstring.h"
#include "gxssl.h"

struct SSLconfig {
  SSLconfig();
  ~SSLconfig();

  gxString ssl_key_file;
  gxString ssl_cert_file;
  gxString ssl_dhparms_file;
  gxString ssh_ca_list_file;
  int ssl_encrypt_backend;
  gxString ssl_backend_hostname;
  int ssl_verify_backend_cert;
  gxString ssl_backend_ca_list_file;
  int ssl_use_dhparms;
  int ssl_handshake_timeout_secs; // Max seconds for a TLS handshake
  gxString ssl_protocol;
  gxString ssl_cipher_list;   // TLS 1.2 ciphers, system crypto policy if empty
  gxString ssl_ciphersuites;  // TLS 1.3 cipher suites, system crypto policy if empty
  gxString ssl_groups;   // 11/18/2026: Key exchange groups, ML-KEM first by default, "system" = crypto policy
  gxString ssl_sigalgs;  // 11/18/2026: Signature algorithms, ML-DSA added by default, "system" = crypto policy
  gxString ssl_crypto_policy_file; // 11/19/2026: RHEL 8 static OpenSSL build only: crypto policy back end ("none" = do not)
  gxSSLMethod meth;
  gxSSL openssl; // Frontend server context
  gxSSL backend; // 10/25/2026: Client context shared by all backend connections
};

int LoadSSLConfig();

extern SSLconfig SSLConfigSruct;
extern SSLconfig *sslcfg;

#endif // __SSH_CONFIG_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
