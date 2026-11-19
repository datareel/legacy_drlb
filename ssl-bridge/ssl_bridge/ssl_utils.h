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

SSL utils for DR load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __SSH_UTILS_HPP__
#define __SSH_UTILS_HPP__

#include "gxdlcode.h"
#include "gxstring.h"

int init_ssl_cert();
int set_groups_sigalgs(SSL_CTX *ctx);
int check_openssl_version();
int apply_crypto_policy(SSL_CTX *ctx, int server, const char *cert_file);
int fips_mode();
int pqc_build();
void openssl_build_info(gxString &s);
int set_ssl_ciphers(SSL_CTX *ctx);

#endif // __SSH_UTILS_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
