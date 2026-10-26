// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 09/17/2016
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

LDM protcol plug in for DR load balancer.

*/
// ----------------------------------------------------------- //   
#ifndef __LDM_PROTO_HPP__
#define __LDM_PROTO_HPP__

#include "gxdlcode.h"
#include "gxstring.h"

#include "ldm_rpc_messages.h"
#include "ldm_feed_types.h"
#include "m_relay.h"
#include "gxlist.h"

struct LDMallow;
struct LDMaccept;

struct LDMrequest {
  LDMrequest() {
    request = 0;
    feed_type = 0;
  }
  ~LDMrequest() { }
  
  void SetFeedType(char net_int[4]);
  unsigned int network_to_int(char net_int[4]);

  int request;

  gxString request_string;
  // HIYA requires ACCEPT entry, and feedtype entry 
  // FEEDME requires ALLOW entry, and feedtype entry

  unsigned int feed_type;
  gxString feed_type_strings;
  gxString client_ip;
  gxString client_host_names;
};


int set_ldm_request_string(int ldm_request, gxString &s);
int set_ldm_feed_type_strings(unsigned int ldm_feed_type, gxString &s);

// 10/25/2026: Convert an ldmd.conf feed type expression, such as
// "IDS|DDPLUS", "ANY-NEXRAD2" or "WMO", to a feed type bit mask.
// Returns 0 if the expression is valid.
int ldm_feed_expr_to_mask(const gxString &expr, unsigned int &mask);

// 10/25/2026: Relay filter that checks every LDM RPC message a frontend
// client sends against the ALLOW and ACCEPT entries in ldmd.conf, and
// writes the optional protocol capture files.
//
// Messages are parsed with their ONC RPC record marks and XDR encoding,
// so checks do not depend on how TCP splits the stream:
// - The first call must be FEEDME, NOTIFYME, HIYA, IS_ALIVE or NULLPROC.
// - FEEDME and NOTIFYME need ALLOW entries for this host that cover
//   every requested feed type of every product specification, and the
//   request patterns must pass the entries' OK and NOT patterns.
// - HIYA needs ACCEPT entries that cover every offered feed type.
// - After HIYA, every HEREIS and COMINGSOON product must match the feed
//   type and prodIdEre of an ACCEPT entry for this host. BLKDATA is only
//   passed after an accepted COMINGSOON.
// - Other calls, or calls out of this order, close the connection.
class LdmFilter : public RelayFilter
{
public:
  LdmFilter();
  ~LdmFilter();

public:
  int Filter(ClientSocket_t *s, int from_frontend, const char *buf, int bytes, MemoryBuffer &out);

private:
  int check_record(ClientSocket_t *s, const unsigned char *p, unsigned len);
  int check_feed_request(ClientSocket_t *s, int proc, const unsigned char *p, unsigned len, unsigned pos);
  int check_hiya(ClientSocket_t *s, const unsigned char *p, unsigned len, unsigned pos);
  int check_product(ClientSocket_t *s, const char *what, const unsigned char *p, unsigned len, unsigned pos);
  void capture(ClientSocket_t *s, int from_frontend, const char *buf, int bytes);
  int deny(ClientSocket_t *s, const gxString &why);
  void match_host_entries(ClientSocket_t *s);

private:
  enum { LDM_START, LDM_FEEDING, LDM_ACCEPTING };
  int state;
  int blkdata_ok;          // Last COMINGSOON was accepted
  MemoryBuffer pending;    // Frontend bytes held until a record can be checked
  unsigned frag_remaining; // Bytes of the current fragment still to pass
  int in_continuation;     // The current record has more fragments
  int capture_fd[2];       // Backend, frontend capture files
  int host_matched;        // host_allow and host_accept are set
  gxList<LDMallow *> host_allow;   // ALLOW entries matching this client
  gxList<LDMaccept *> host_accept; // ACCEPT entries matching this client
};

#endif // __LDM_PROTO_HPP__
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
