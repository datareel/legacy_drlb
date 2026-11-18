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

SSLconfig SSLConfigSruct;
SSLconfig *sslcfg = &SSLConfigSruct;

SSLconfig::SSLconfig()
{
  ssl_key_file = "/etc/drlb/ssl/private/ca.key";
  ssl_cert_file =  "/etc/drlb/ssl/certs/ca.crt";
  ssl_dhparms_file =  "/etc/drlb/ssl/certs/dhparams.pem";
  ssl_backend_ca_list_file = "/etc/pki/tls/certs/ca-bundle.crt";
  ssl_encrypt_backend = 0;
  ssl_backend_hostname.Clear();
  ssl_verify_backend_cert = 1; // 10/25/2026: Backend certificates are verified by default
  ssh_ca_list_file.Clear();
  ssl_use_dhparms = 0;
  ssl_handshake_timeout_secs = 30;
  meth =  gxSSL_TLS;
  ssl_protocol = "TLS";
  ssl_cipher_list.Clear();
  ssl_ciphersuites.Clear();
  ssl_groups.Clear();
  ssl_sigalgs.Clear();
}

SSLconfig::~SSLconfig()
{

}

int LoadSSLConfig()
{
  NT_print("Loading SSL configuration");
  
  const size_t linelen = 1024;
  char linebuf[linelen];
  DiskFileB ifile;
  ifile.df_Open(servercfg->config_file.c_str());
  if(!ifile) {
    NT_print("ERROR - Cannot open config file for reading", servercfg->config_file.c_str());
    NT_print(ifile.df_ExceptionMessage());
    return 1;
  }
  
  UString info_line, sbuf, parm_name;
  int error_level = 0;
  gxList<gxString> filelist;
  char pbuf[CFG_VALUE_SIZE];
  int filter_linefeeds = 0;

  while(!ifile.df_EOF()) {
    memset(linebuf, 0, linelen);
    ifile.df_GetLine(linebuf, linelen, '\n', filter_linefeeds);
    if(ifile.df_GetError() != DiskFileB::df_NO_ERROR) {
      NT_print("ERROR - A fatal I/O error reading", servercfg->config_file.c_str());
      NT_print(ifile.df_ExceptionMessage());
      error_level = 1;
      break;
    }
    info_line << clear << linebuf;
    filelist.Add(info_line);
  }
  ifile.df_Close();

  if(error_level != 0) return error_level;

  int has_ssl_config_section = 0;
  gxListNode<gxString> *ptr = filelist.GetHead();
  while(ptr) {
    if(ptr->data.IFind("[SSLCONFIG]") != -1) {
      has_ssl_config_section = 1;
      break;
    }
    ptr = ptr->next;
  }

  if(!has_ssl_config_section) {
    NT_print("INFO - DRLB config file does not have and [SSLCONFIG] section:", servercfg->config_file.c_str());
    NT_print("INFO - Adding [SSLCONFIG] section to DRLB server config:", servercfg->config_file.c_str());
    DiskFileB dfile(servercfg->config_file.c_str(), DiskFileB::df_READWRITE, DiskFileB::df_CREATE); 
    if(!dfile) {
      NT_print("ERROR - Cannot write to config file", servercfg->config_file.c_str());
      return 1;
    }
    dfile << "#" << "\n";
    dfile << "\n";
    dfile << "# DRLB server added SSLCONFIG section to config file" << "\n";
    dfile << "[SSLCONFIG]" << "\n";
    dfile << "# SSL bridge config" << "\n";
    dfile << "#" << "\n";
    dfile << "# TLS protocol versions" << "\n";
    dfile << "# TLS - TLS 1.2 and TLS 1.3 (default)" << "\n";
    dfile << "# TLSv1_2 - TLS 1.2 only" << "\n";
    dfile << "# TLSv1_3 - TLS 1.3 only" << "\n";
    dfile << "ssl_protocol = TLS" << "\n";
    dfile << "#" << "\n";
    dfile << "# Ciphers follow the system crypto policy (update-crypto-policies)." << "\n";
    dfile << "# Set these only to restrict them further for this server." << "\n";
    dfile << "##ssl_cipher_list = ECDHE+AESGCM:ECDHE+CHACHA20" << "\n";
    dfile << "##ssl_ciphersuites = TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256" << "\n";
    dfile << "#" << "\n";
    dfile << "# Key exchange groups. The default puts the hybrid post-quantum ML-KEM" << "\n";
    dfile << "# groups first (X25519MLKEM768, SecP256r1MLKEM768, SecP384r1MLKEM1024)," << "\n";
    dfile << "# then X25519, P-256, X448, P-384, P-521, ffdhe2048/3072." << "\n";
    dfile << "# system = use the crypto policy list (the default in FIPS mode)." << "\n";
    dfile << "##ssl_groups = X25519MLKEM768:X25519:secp256r1" << "\n";
    dfile << "# Signature algorithms. The default adds ML-DSA to the ECDSA, EdDSA and" << "\n";
    dfile << "# RSA algorithms. system = use the crypto policy list." << "\n";
    dfile << "##ssl_sigalgs = system" << "\n";
    dfile << "#" << "\n";
    dfile << "# Seconds allowed for a TLS handshake" << "\n";
    dfile << "ssl_handshake_timeout_secs = 30" << "\n";
    dfile << "#" << "\n";
    dfile << "# Private key file" << "\n";
    dfile << "ssl_key_file = /etc/drlb/ssl/private/ca.key" << "\n";
    dfile << "#" << "\n";
    dfile << "# SSL cert file" << "\n";
    dfile << "ssl_cert_file =  /etc/drlb/ssl/certs/ca.crt" << "\n";
    dfile << "#" << "\n";
    dfile << "# Certificate chain" << "\n";
    dfile << "##ssh_ca_list_file = /etc/drlb/ssl/certs/ca_list.crt" << "\n";
    dfile << "#" << "\n";
    dfile << "# Optional Diffie-Hellman parameters" << "\n";
    dfile << "##ssl_use_dhparms = 0" << "\n";
    dfile << "##ssl_dhparms_file =  /etc/drlb/ssl/certs/dhparams.pem" << "\n";
    dfile << "#" << "\n";
    dfile << "# Client side settings for backend SSL encryption" << "\n";
    dfile << "# Use SSL to connect to LB nodes" << "\n";
    dfile << "##ssl_encrypt_backend = 0" << "\n";
    dfile << "# Backend certificates are verified against the CA list and the node" << "\n";
    dfile << "# hostname, or ssl_backend_hostname if set, using subjectAltName." << "\n";
    dfile << "# Set ssl_verify_backend_cert = 0 only for test backends." << "\n";
    dfile << "##ssl_backend_hostname = www.example.com" << "\n";
    dfile << "##ssl_verify_backend_cert = 1" << "\n";
    dfile << "##ssl_backend_ca_list_file = /etc/pki/tls/certs/ca-bundle.crt" << "\n";

    dfile << "\n";
    ptr = filelist.GetHead();
    while(ptr) {
      dfile << ptr->data.c_str();
      ptr = ptr->next;
    }
    dfile.df_Close();
  }
  else {
    NT_print("INFO - Reading [SSLCONFIG] config section from DRLB config file:", servercfg->config_file.c_str());
    ptr = filelist.GetHead();
    has_ssl_config_section = 0;
    while(ptr) {
      info_line << clear << ptr->data;
      info_line.FilterChar('\n');    
      info_line.FilterChar('\r');    
      info_line.TrimLeadingSpaces();
      info_line.TrimTrailingSpaces();
      if(info_line[0] == '#') { ptr = ptr->next; continue; } 
      if(info_line[0] == ';') { ptr = ptr->next; continue; } 
      info_line.DeleteAfterIncluding("#");
      info_line.TrimTrailingSpaces();
      if(info_line.is_null()) { ptr = ptr->next; continue; } 
      
      gxString strdup = ptr->data;
      strdup.FilterChar(' ');
      if(strdup.IFind("[SSLCONFIG]") != -1) {
	has_ssl_config_section++;
	ptr = ptr->next;
	while(ptr) {
	  if(ptr->data[0] == '[') {
	    ptr = ptr->prev;
	    break;
	  }
	  info_line << clear << ptr->data;
	  info_line.FilterChar('\n');    
	  info_line.FilterChar('\r');    
	  info_line.TrimLeadingSpaces();
	  info_line.TrimTrailingSpaces();
	  if(info_line[0] == '#') { ptr = ptr->next; continue; } 
	  if(info_line[0] == ';') { ptr = ptr->next; continue; } 
	  info_line.DeleteAfterIncluding("#");
	  info_line.TrimTrailingSpaces();
	  if(info_line.is_null()) { ptr = ptr->next; continue; } 

	  // Add all [SSLCONFIG] reads here
	  if(info_line.IFind("ssl_key_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_key_file") {
	      sslcfg->ssl_key_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_cert_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_cert_file") {
	      sslcfg->ssl_cert_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssh_ca_list_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssh_ca_list_file") {
	      sslcfg->ssh_ca_list_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_encrypt_backend") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_encrypt_backend") {
	      sslcfg->ssl_encrypt_backend = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_verify_backend_cert") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_verify_backend_cert") {
	      sslcfg->ssl_verify_backend_cert = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_backend_hostname") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_backend_hostname") {
	      sslcfg->ssl_backend_hostname = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_backend_ca_list_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_backend_ca_list_file") {
	      sslcfg->ssl_backend_ca_list_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_use_dhparms") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_use_dhparms") {
	      sslcfg->ssl_use_dhparms = CfgGetTrueFalseValue(info_line);
	    }
	  }
	  if(info_line.IFind("ssl_dhparms_file") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_dhparms_file") {
	      sslcfg->ssl_dhparms_file = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_handshake_timeout_secs") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_handshake_timeout_secs") {
	      sslcfg->ssl_handshake_timeout_secs = CfgGetEqualToParm(info_line);
	      if(sslcfg->ssl_handshake_timeout_secs <= 0) {
		NT_print("ERROR - Bad ssl_handshake_timeout_secs value in [SSLCONFIG]");
		error_level = 1;
	      }
	    }
	  }
	  if(info_line.IFind("ssl_cipher_list") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_cipher_list") {
	      sslcfg->ssl_cipher_list = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_ciphersuites") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_ciphersuites") {
	      sslcfg->ssl_ciphersuites = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_groups") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_groups") {
	      sslcfg->ssl_groups = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_sigalgs") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_sigalgs") {
	      sslcfg->ssl_sigalgs = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(info_line.IFind("ssl_protocol") != -1) {
	    CfgGetParmName(info_line, parm_name); parm_name.ToLower();
	    if(parm_name == "ssl_protocol") {
	      sslcfg->ssl_protocol = CfgGetEqualToParm(info_line, pbuf);
	    }
	  }
	  if(!ptr) break;
	  ptr = ptr->next;
	}
      }
      if(!ptr) break;
      ptr = ptr->next;
    }
    if(has_ssl_config_section > 1) {
      sbuf << clear << "ERROR - DRLB server config has " << has_ssl_config_section 
	   << " [SSLCONFIG] sections " << servercfg->config_file;
      NT_print(sbuf.c_str());
      return 1;
    }
  }

  return error_level;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
