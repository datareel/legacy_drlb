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

LDM protcol plug in for DR load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "drlb_server.h"
#include "ldm_proto.h"
#include "ldm_config.h"

unsigned int LDMrequest::network_to_int(char net_int[4])
{
  unsigned int buf = 0;
  unsigned int d = (unsigned int)net_int[0];
  unsigned int c = (unsigned int)net_int[1];
  unsigned int b = (unsigned int)net_int[2];
  unsigned int a = (unsigned int)net_int[3];
  a = a & 0xFF;
  b = (b<<8) & 0xFF00;
  c = (c<<16) & 0xFF0000;
  d = (d<<24) & 0xFF000000;
  buf = a + b + c + d;
  return buf;
}

void LDMrequest::SetFeedType(char net_int[4]) 
{
  feed_type = network_to_int(net_int);
}

int set_ldm_request_string(int ldm_request, gxString &s)
{
  s.Clear();
  int error_level = 0;

  switch(ldm_request) {
    case 0:
      s = "NONE";
      break;
    case HEREIS:
      s = "HEREIS";
      break;
    case FEEDME:
      s = "FEEDME";
      break;
    case HIYA: 
      s = "HIYA";
      break;
    case NOTIFICATION:
      s = "NOTIFICATION";
      break;
    case NOTIFYME:
      s = "NOTIFYME";
      break;
    case COMINGSOON:
      s = "COMINGSOON";
      break;
    case BLKDATA:
      s = "BLKDATA";
      break;
    case IS_ALIVE:
      s = "IS_ALIVE";
      break;
    default:
      s = "UNKNOWN";
      error_level = -1;
      break;
  }

  return error_level;
}

int set_ldm_feed_type_strings(unsigned int ldm_feed_type, gxString &s)
{
  s.Clear();
  int error_level = 0;
  int has_feed_type = 0;

  if(ldm_feed_type == 0) {
    s = "NONE";
    return 1; // Cannot have NONE as a feed type
  }

  if(ldm_feed_type == ANY) {
    s = "ANY";
    return 0;
  }

  if((FT0 & ldm_feed_type) == FT0) {
    if(!has_feed_type) {
      s = "PPS,FT0";
    }
    else {
      s << "," << "PPS,FT0";
    }
    has_feed_type = 1;
  }

  if((FT1 & ldm_feed_type) == FT1) {
    if(!has_feed_type) {
      s = "DDS,FT1,DOMESTIC";
    }
    else {
      s << "," << "DDS,FT1,DOMESTIC";
    }
    has_feed_type = 1;
  }

  if((DDPLUS & ldm_feed_type) == DDPLUS) {
    if(!has_feed_type) {
      s = "DDPLUS";
    }
    else {
      s << "," << "DDPLUS";
    }
    has_feed_type = 1;
  }
  
  if((FT2 & ldm_feed_type) == FT2) {
    if(!has_feed_type) {
      s = "HDS,FT2,HRS";
    }
    else {
      s << "," << "HDS,FT2,HRS";
    }
    has_feed_type = 1;
  }
      
  if((FT3 & ldm_feed_type) == FT3) {
    if(!has_feed_type) {
      s = "IDS,FT3,INTNL";
    }
    else {
      s << "," << "IDS,FT3,INTNL";
    }
    has_feed_type = 1;
  }
      
  if((FT4 & ldm_feed_type) == FT4) {
    if(!has_feed_type) {
      s = "SPARE,FT4";
    }
    else {
      s << "," << "SPARE,FT4";
    }
    has_feed_type = 1;
  }
      
  if((WMO & ldm_feed_type) == WMO) {
    if(!has_feed_type) {
      s = "WMO";
    }
    else {
      s << "," << "WMO";
    }
    has_feed_type = 1;
  }
      
  if((FT5 & ldm_feed_type) == FT5) {
    if(!has_feed_type) {
      s = "UNIWISC,FT5,MCIDAS";
    }
    else {
      s << "," << "UNIWISC,FT5,MCIDAS";
    }
    has_feed_type = 1;
  }
  
  if((UNIDATA & ldm_feed_type) == UNIDATA) {
    if(!has_feed_type) {
      s = "UNIDATA";
    }
    else {
      s << "," << "UNIDATA";
    }
    has_feed_type = 1;
  }

  if((FT6 & ldm_feed_type) == FT6) {
    if(!has_feed_type) {
      s = "PCWS,FT6,ACARS";
    }
    else {
      s << "," << "PCWS,FT6,ACARS";
    }
    has_feed_type = 1;
  }
      
  if((FT7 & ldm_feed_type) == FT7) {
    if(!has_feed_type) {
      s = "FSL2,FT7,PROFILER";
    }
    else {
      s << "," << "FSL2,FT7,PROFILER";
    }
    has_feed_type = 1;
  }
      
  if((FT8 & ldm_feed_type) == FT8) {
    if(!has_feed_type) {
      s = "FSL3,FT8";
    }
    else {
      s << "," << "FSL3,FT8";
    }
    has_feed_type = 1;
  }
      
  if((FT9 & ldm_feed_type) == FT9) {
    if(!has_feed_type) {
      s = "FSL4,FT9";
    }
    else {
      s << "," << "FSL4,FT9";
    }
    has_feed_type = 1;
  }
      
  if((FT10 & ldm_feed_type) == FT10) {
    if(!has_feed_type) {
      s = "FSL5,FT10";
    }
    else {
      s << "," << "FSL5,FT10";
    }
    has_feed_type = 1;
  }
  
  if((FSL & ldm_feed_type) == FSL) {
    if(!has_feed_type) {
      s = "FSL";
    }
    else {
      s << "," << "FSL";
    }
    has_feed_type = 1;
  }
      
  if((FT11 & ldm_feed_type) == FT11) {
    if(!has_feed_type) {
      s = "GPSSRC,FT11,NMC1,AFOS";
    }
    else {
      s << "," << "GPSSRC,FT11,NMC1,AFOS";
    }
    has_feed_type = 1;
  }
      
  if((FT12 & ldm_feed_type) == FT12) {
    if(!has_feed_type) {
      s = "CONDUIT,FT12,NMC2,NCEPH";
    }
    else {
      s << "," << "CONDUIT,FT12,NMC2,NCEPH";
    }
    has_feed_type = 1;
  }
      
  if((FT13 & ldm_feed_type) == FT13) {
    if(!has_feed_type) {
      s = "FNEXRAD,FT13,NMC3";
    }
    else {
      s << "," << "FNEXRAD,FT13,NMC3";
    }
    has_feed_type = 1;
  }
      
  if((NMC & ldm_feed_type) == NMC) {
    if(!has_feed_type) {
      s = "NMC";
    }
    else {
      s << "," << "NMC";
    }
    has_feed_type = 1;
  }
      
  if((FT14 & ldm_feed_type) == FT14) {
    if(!has_feed_type) {
      s = "LIGHTNING,FT14,NLDN";
    }
    else {
      s << "," << "LIGHTNING,FT14,NLDN";
    }
    has_feed_type = 1;
  }
      
  if((FT15 & ldm_feed_type) == FT15) {
    if(!has_feed_type) {
      s = "WSI,FT15";
    }
    else {
      s << "," << "WSI,FT15";
    }
    has_feed_type = 1;
  }
      
  if((FT16 & ldm_feed_type) == FT16) {
    if(!has_feed_type) {
      s = "DIFAX,FT16";
    }
    else {
      s << "," << "DIFAX,FT16";
    }
    has_feed_type = 1;
  }
  
  if((FT17 & ldm_feed_type) == FT17) {
    if(!has_feed_type) {
      s = "FAA604,FT17,FAA,604";
    }
    else {
      s << "," << "FAA604,FT17,FAA,604";
    }
    has_feed_type = 1;
  }
  
  if((FT18 & ldm_feed_type) == FT18) {
    if(!has_feed_type) {
      s = "GPS,FT18";
    }
    else {
      s << "," << "GPS,FT18";
    }
    has_feed_type = 1;
  }
      
  if((FT19 & ldm_feed_type) == FT19) {
    if(!has_feed_type) {
      s = "FNMOC,FT19,SEISMIC,NOGAPS";
    }
    else {
      s << "," << "FNMOC,FT19,SEISMIC,NOGAPS";
    }
    has_feed_type = 1;
  }
      
  if((FT20 & ldm_feed_type) == FT20) {
    if(!has_feed_type) {
      s = "GEM,FT20,CMC";
    }
    else {
      s << "," << "GEM,FT20,CMC";
    }
    has_feed_type = 1;
  }
      
  if((FT21 & ldm_feed_type) == FT21) {
    if(!has_feed_type) {
      s = "NIMAGE,FT21,IMAGE";
    }
    else {
      s << "," << "NIMAGE,FT21,IMAGE";
    }
    has_feed_type = 1;
  }
  
  if((FT22 & ldm_feed_type) == FT22) {
    if(!has_feed_type) {
      s = "NTEXT,FT22,TEXT";
    }
    else {
      s << "," << "NTEXT,FT22,TEXT";
    }
    has_feed_type = 1;
  }
      
  if((FT23 & ldm_feed_type) == FT23) {
    if(!has_feed_type) {
      s = "NGRID,FT23,GRID";
    }
    else {
      s << "," << "NGRID,FT23,GRID";
    }
    has_feed_type = 1;
  }
      
  if((FT24 & ldm_feed_type) == FT24) {
    if(!has_feed_type) {
      s = "NPOINT,FT24,POINT,NBUFR,BUFR";
    }
    else {
      s << "," << "NPOINT,FT24,POINT,NBUFR,BUFR";
    }
    has_feed_type = 1;
  }
      
  if((FT25 & ldm_feed_type) == FT25) {
    if(!has_feed_type) {
      s = "NGRAPH,FT25,GRAPH";
    }
    else {
      s << "," << "NGRAPH,FT25,GRAPH";
    }
    has_feed_type = 1;
  }
      
  if((FT26 & ldm_feed_type) == FT26) {
    if(!has_feed_type) {
      s = "NOTHER,FT26,OTHER";
    }
    else {
      s << "," << "NOTHER,FT26,OTHER";
    }
    has_feed_type = 1;
  }
      
  if((NPORT & ldm_feed_type) == NPORT) {
    if(!has_feed_type) {
      s = "NPORT";
    }
    else {
      s << "," << "NPORT";
    }
    has_feed_type = 1;
  }
      
  if((FT27 & ldm_feed_type) == FT27) {
    if(!has_feed_type) {
      s = "NEXRAD3,FT27,NNEXRAD,NEXRAD";
    }
    else {
      s << "," << "NEXRAD3,FT27,NNEXRAD,NEXRAD";
    }
    has_feed_type = 1;
  }
      
  if((FT28 & ldm_feed_type) == FT28) {
    if(!has_feed_type) {
      s = "NEXRAD2,FT28,CRAFT,NEXRD2";
    }
    else {
      s << "," << "NEXRAD2,FT28,CRAFT,NEXRD2";
    }
    has_feed_type = 1;
  }
  
  if((FT29 & ldm_feed_type) == FT29) { 
    if(!has_feed_type) {
      s = "NXRDSRC,FT29";
    }
    else {
      s << "," << "NXRDSRC,FT29";
    }
    has_feed_type = 1;
  }
      
  if((EXP & ldm_feed_type) == EXP) { 
    if(!has_feed_type) {
      s = "EXP,FT30";
    }
    else {
      s << "," << "EXP,FT30";
    }
    has_feed_type = 1;
  }
      
  if(!has_feed_type) {
    s = "UNKNOWN";
    error_level = -1;
  }

  return error_level;
}
// ----------------------------------------------------------- //
// 10/25/2026: LDM feed type expressions and the LDM RPC filter
// ----------------------------------------------------------- //
struct ldm_feed_name {
  const char *name;
  unsigned int mask;
};

// Feed type names from the LDM protocol definition (ldm.x)
static const ldm_feed_name ldm_feed_names[] = {
  {"NONE", 0x0}, {"FT0", 0x1}, {"PPS", 0x1}, {"FT1", 0x2}, {"DDS", 0x2}, {"DOMESTIC", 0x2},
  {"DDPLUS", 0x3}, {"FT2", 0x4}, {"HDS", 0x4}, {"HRS", 0x4}, {"FT3", 0x8}, {"IDS", 0x8},
  {"INTNL", 0x8}, {"FT4", 0x10}, {"SPARE", 0x10}, {"WMO", 0xF}, {"FT5", 0x20}, {"UNIWISC", 0x20},
  {"MCIDAS", 0x20}, {"UNIDATA", 0x2F}, {"FT6", 0x40}, {"PCWS", 0x40}, {"ACARS", 0x40},
  {"FT7", 0x80}, {"FSL2", 0x80}, {"PROFILER", 0x80}, {"FT8", 0x100}, {"FSL3", 0x100},
  {"FT9", 0x200}, {"FSL4", 0x200}, {"FT10", 0x400}, {"FSL5", 0x400}, {"FSL", 0x7C0},
  {"FT11", 0x800}, {"AFOS", 0x800}, {"GPSSRC", 0x800}, {"NMC1", 0x800}, {"FT12", 0x1000},
  {"CONDUIT", 0x1000}, {"NMC2", 0x1000}, {"NCEPH", 0x1000}, {"FT13", 0x2000}, {"NMC3", 0x2000},
  {"FNEXRAD", 0x2000}, {"NMC", 0x3800}, {"FT14", 0x4000}, {"NLDN", 0x4000}, {"LIGHTNING", 0x4000},
  {"FT15", 0x8000}, {"WSI", 0x8000}, {"FT16", 0x10000}, {"DIFAX", 0x10000}, {"SATELLITE", 0x10000},
  {"FT17", 0x20000}, {"FAA604", 0x20000}, {"FAA", 0x20000}, {"604", 0x20000},
  {"FT18", 0x40000}, {"GPS", 0x40000}, {"FT19", 0x80000}, {"SEISMIC", 0x80000},
  {"NOGAPS", 0x80000}, {"FNMOC", 0x80000}, {"FT20", 0x100000}, {"CMC", 0x100000},
  {"GEM", 0x100000}, {"FT21", 0x200000}, {"NIMAGE", 0x200000}, {"IMAGE", 0x200000},
  {"FT22", 0x400000}, {"NTEXT", 0x400000}, {"TEXT", 0x400000}, {"FT23", 0x800000},
  {"NGRID", 0x800000}, {"GRID", 0x800000}, {"FT24", 0x1000000}, {"NPOINT", 0x1000000},
  {"POINT", 0x1000000}, {"NBUFR", 0x1000000}, {"BUFR", 0x1000000}, {"FT25", 0x2000000},
  {"NGRAPH", 0x2000000}, {"GRAPH", 0x2000000}, {"FT26", 0x4000000}, {"NOTHER", 0x4000000},
  {"OTHER", 0x4000000}, {"NPORT", 0x7C00000}, {"FT27", 0x8000000}, {"NEXRAD3", 0x8000000},
  {"NNEXRAD", 0x8000000}, {"NEXRAD", 0x8000000}, {"FT28", 0x10000000}, {"CRAFT", 0x10000000},
  {"NEXRD2", 0x10000000}, {"NEXRAD2", 0x10000000}, {"FT29", 0x20000000}, {"NXRDSRC", 0x20000000},
  {"FT30", 0x40000000}, {"EXP", 0x40000000}, {"ANY", 0xFFFFFFFF},
  {0, 0}
};

static int ldm_feed_name_to_mask(const gxString &name, unsigned int &mask)
{
  gxString n = name;
  n.TrimLeadingSpaces(); n.TrimTrailingSpaces();
  n.TrimLeading('('); n.TrimTrailing(')');
  if(n.is_null()) return 1;
  for(int i = 0; ldm_feed_names[i].name; i++) {
    if(n == ldm_feed_names[i].name) {
      mask = ldm_feed_names[i].mask;
      return 0;
    }
  }
  // A number, decimal or 0x hex
  char *end = 0;
  unsigned long v = strtoul(n.c_str(), &end, 0);
  if(end && *end == 0) {
    mask = (unsigned int)v;
    return 0;
  }
  return 1;
}

int ldm_feed_expr_to_mask(const gxString &expr, unsigned int &mask)
{
  // Terms are joined with | (add) and - (remove), evaluated left to right
  gxString e = expr;
  e.ToUpper();
  e.FilterChar(' '); e.FilterChar('\t');
  mask = 0;
  if(e.is_null()) return 1;
  const char *p = e.c_str();
  int op = '|';
  gxString term;
  while(1) {
    if(*p == '|' || *p == '-' || *p == 0) {
      unsigned int m = 0;
      if(ldm_feed_name_to_mask(term, m) != 0) return 1;
      if(op == '|') mask |= m; else mask &= ~m;
      if(*p == 0) break;
      op = *p;
      term.Clear();
    }
    else {
      term << *p;
    }
    p++;
  }
  return 0;
}

// Bounds checked XDR reads. Each returns 0 if the value was read.
static int xdr_u32(const unsigned char *p, unsigned len, unsigned &pos, unsigned int &v)
{
  if(pos > len || len - pos < 4) return 1;
  v = ((unsigned int)p[pos] << 24) | ((unsigned int)p[pos+1] << 16) | ((unsigned int)p[pos+2] << 8) | p[pos+3];
  pos += 4;
  return 0;
}

static int xdr_skip(unsigned len, unsigned &pos, unsigned n)
{
  if(pos > len || len - pos < n) return 1;
  pos += n;
  return 0;
}

static int xdr_string(const unsigned char *p, unsigned len, unsigned &pos, unsigned max, gxString &s)
{
  unsigned int n;
  if(xdr_u32(p, len, pos, n) != 0) return 1;
  if(n > max) return 1;
  unsigned padded = (n + 3) & ~3U;
  if(pos > len || len - pos < padded) return 1;
  s.Clear();
  if(n > 0) s.InsertAt(0, (const char *)p + pos, n);
  pos += padded;
  return 0;
}

static int xdr_opaque_auth(unsigned len, unsigned &pos, const unsigned char *p)
{
  unsigned int flavor, n;
  if(xdr_u32(p, len, pos, flavor) != 0) return 1;
  if(xdr_u32(p, len, pos, n) != 0) return 1;
  if(n > 400) return 1; // RFC 5531 limit
  return xdr_skip(len, pos, (n + 3) & ~3U);
}

static const unsigned LDM_PROG = 300029;
static const unsigned LDM_CHECK_BYTES = 16384; // Enough for any LDM call header and product info

LdmFilter::LdmFilter()
{
  state = LDM_START;
  blkdata_ok = 0;
  frag_remaining = 0;
  in_continuation = 0;
  host_matched = 0;
  capture_fd[0] = capture_fd[1] = -1;
}

LdmFilter::~LdmFilter()
{
  for(int i = 0; i < 2; i++) {
    if(capture_fd[i] >= 0) close(capture_fd[i]);
    capture_fd[i] = -1;
  }
}

void LdmFilter::match_host_entries(ClientSocket_t *s)
// Find the ALLOW and ACCEPT entries for this client once per connection,
// so DNS lookups are not repeated for every message
{
  if(host_matched) return;
  host_matched = 1;
  gxListNode<LDMallow> *al = ldmcfg->ldm_allow_list.GetHead();
  while(al) {
    int has_match = 0;
    compare_ldm_hostere_to_hostip(al->data.hostIdEre, s->client_name, s->seq_num, has_match, ldmcfg->resolve_ldm_hostnames);
    LDMallow *ap = &al->data;
    if(has_match) host_allow.Add(ap);
    al = al->next;
  }
  gxListNode<LDMaccept> *ac = ldmcfg->ldm_accept_list.GetHead();
  while(ac) {
    int has_match = 0;
    compare_ldm_hostere_to_hostip(ac->data.hostIdEre, s->client_name, s->seq_num, has_match, ldmcfg->resolve_ldm_hostnames);
    LDMaccept *ap = &ac->data;
    if(has_match) host_accept.Add(ap);
    ac = ac->next;
  }
}

int LdmFilter::deny(ClientSocket_t *s, const gxString &why)
{
  gxString message;
  message << clear << "[" << s->seq_num << "]: " << why << " from " << s->client_name;
  LogMessage(message.c_str());
  return 1;
}

void LdmFilter::capture(ClientSocket_t *s, int from_frontend, const char *buf, int bytes)
{
  int enabled = from_frontend ? ldmcfg->enable_frontend_proto_capture : ldmcfg->enable_backend_proto_capture;
  if(!enabled || bytes <= 0) return;
  int i = from_frontend ? 1 : 0;
  if(capture_fd[i] == -1) {
    // Created with O_EXCL and O_NOFOLLOW in a directory only this user can
    // write, so a planted symlink or file cannot redirect the capture
    mkdir(ldmcfg->proto_capture_dir.c_str(), 0700);
    gxString fname;
    fname << clear << ldmcfg->proto_capture_dir << "/" << (from_frontend ? "frontend" : "backend")
	  << "_capture_" << (unsigned long)time(0) << "_" << s->seq_num << ".bin";
    capture_fd[i] = open(fname.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if(capture_fd[i] < 0) {
      gxString message;
      message << clear << "[" << s->seq_num << "]: ERROR - Cannot create protocol capture file " << fname;
      LogMessage(message.c_str());
      capture_fd[i] = -2; // Do not retry
      return;
    }
  }
  if(capture_fd[i] >= 0) {
    if(write(capture_fd[i], buf, bytes) != bytes) {
      close(capture_fd[i]);
      capture_fd[i] = -2;
    }
  }
}

int LdmFilter::Filter(ClientSocket_t *s, int from_frontend, const char *buf, int bytes, RelayBuffer &out)
{
  capture(s, from_frontend, buf, bytes);
  if(!from_frontend) { // Data from the backend LDM server is passed as is
    out.Cat(buf, bytes);
    return 0;
  }

  pending.Cat(buf, bytes);
  while(pending.length() > 0) {
    const unsigned char *p = (const unsigned char *)pending.m_buf();
    unsigned avail = (unsigned)pending.length();
    if(frag_remaining > 0) { // Pass the rest of a checked fragment
      unsigned n = (frag_remaining < avail) ? frag_remaining : avail;
      out.Cat(p, n);
      pending.DeleteFront(n);
      frag_remaining -= n;
      continue;
    }
    // Start of a fragment: 4 byte record mark
    if(avail < 4) break;
    unsigned int mark = ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
    int last = (mark & 0x80000000U) != 0;
    unsigned frag_len = mark & 0x7FFFFFFFU;
    if(!in_continuation) {
      // First fragment of a record: hold it until the call header and
      // arguments can be checked
      unsigned need = (frag_len < LDM_CHECK_BYTES) ? frag_len : LDM_CHECK_BYTES;
      if(avail - 4 < need) break; // Wait for more data
      if(check_record(s, p + 4, need) != 0) return 1;
    }
    in_continuation = !last;
    out.Cat(p, 4);
    pending.DeleteFront(4);
    frag_remaining = frag_len;
  }
  return 0;
}

int LdmFilter::check_record(ClientSocket_t *s, const unsigned char *p, unsigned len)
// Check one RPC message. len may be shorter than the whole message.
{
  gxString why;
  unsigned pos = 0;
  unsigned int xid, mtype;
  if(xdr_u32(p, len, pos, xid) != 0 || xdr_u32(p, len, pos, mtype) != 0) {
    why << clear << "LDM DENIED - message too short";
    return deny(s, why);
  }
  if(mtype == 1) { // REPLY to a call the backend sent on this connection
    if(state == LDM_START) {
      why << clear << "LDM DENIED - RPC reply before any request";
      return deny(s, why);
    }
    return 0;
  }
  if(mtype != 0) {
    why << clear << "LDM DENIED - bad RPC message type " << (unsigned long)mtype;
    return deny(s, why);
  }
  unsigned int rpcvers, prog, vers, proc;
  if(xdr_u32(p, len, pos, rpcvers) || xdr_u32(p, len, pos, prog) || 
     xdr_u32(p, len, pos, vers) || xdr_u32(p, len, pos, proc) ||
     xdr_opaque_auth(len, pos, p) || xdr_opaque_auth(len, pos, p)) {
    why << clear << "LDM DENIED - bad RPC call header";
    return deny(s, why);
  }
  if(rpcvers != 2 || prog != LDM_PROG || (vers != 5 && vers != 6)) {
    why << clear << "LDM DENIED - not an LDM 5 or 6 call (program " << (unsigned long)prog 
	<< " version " << (unsigned long)vers << ")";
    return deny(s, why);
  }

  gxString name;
  if(set_ldm_request_string(proc, name) != 0) name << clear << (unsigned long)proc;
  if(servercfg->debug && servercfg->debug_level >= 2) {
    gxString message;
    message << clear << "[" << s->seq_num << "]: Received ldm rpc " << name << " message from " << s->client_name;
    LogDebugMessage(message.c_str());
  }

  switch(proc) {
    case 0: // NULLPROC
    case IS_ALIVE:
      return 0;
    case FEEDME:
    case NOTIFYME:
      if(state != LDM_START) {
	why << clear << "ALLOW DENIED - " << name << " after the session started";
	return deny(s, why);
      }
      if(proc == FEEDME && vers == 6) {
	unsigned int present;
	if(xdr_u32(p, len, pos, present) != 0 || present != 1) {
	  why << clear << "ALLOW DENIED - FEEDME without a product class";
	  return deny(s, why);
	}
      }
      if(check_feed_request(s, proc, p, len, pos) != 0) return 1;
      state = LDM_FEEDING;
      return 0;
    case HIYA:
      if(state != LDM_START) {
	why << clear << "ACCEPT DENIED - HIYA after the session started";
	return deny(s, why);
      }
      if(check_hiya(s, p, len, pos) != 0) return 1;
      state = LDM_ACCEPTING;
      return 0;
    case HEREIS:
      if(state != LDM_ACCEPTING) {
	why << clear << "ACCEPT DENIED - HEREIS without an accepted HIYA";
	return deny(s, why);
      }
      return check_product(s, "HEREIS", p, len, pos);
    case COMINGSOON: {
      if(state != LDM_ACCEPTING) {
	why << clear << "ACCEPT DENIED - COMINGSOON without an accepted HIYA";
	return deny(s, why);
      }
      unsigned int present;
      blkdata_ok = 0;
      if(xdr_u32(p, len, pos, present) != 0 || present != 1) {
	why << clear << "ACCEPT DENIED - COMINGSOON without product information";
	return deny(s, why);
      }
      if(check_product(s, "COMINGSOON", p, len, pos) != 0) return 1;
      blkdata_ok = 1;
      return 0;
    }
    case BLKDATA:
      if(state != LDM_ACCEPTING || !blkdata_ok) {
	why << clear << "ACCEPT DENIED - BLKDATA without an accepted COMINGSOON";
	return deny(s, why);
      }
      return 0;
    default:
      why << clear << "LDM DENIED - " << name << " is not allowed from a client";
      return deny(s, why);
  }
}

// Parse the product specifications of a prod_class_t. Calls back for
// each feed type and pattern. Returns 0 if the class was read.
struct ldm_prod_spec {
  unsigned int feedtype;
  gxString pattern;
};

static int read_prod_class(const unsigned char *p, unsigned len, unsigned &pos, gxList<ldm_prod_spec> &specs)
{
  unsigned int n;
  if(xdr_skip(len, pos, 16) != 0) return 1; // from and to times
  if(xdr_u32(p, len, pos, n) != 0) return 1;
  if(n == 0 || n > 32) return 1; // PSA_MAX
  for(unsigned i = 0; i < n; i++) {
    ldm_prod_spec spec;
    if(xdr_u32(p, len, pos, spec.feedtype) != 0) return 1;
    if(xdr_string(p, len, pos, 255, spec.pattern) != 0) return 1;
    specs.Add(spec);
  }
  return 0;
}

int LdmFilter::check_feed_request(ClientSocket_t *s, int proc, const unsigned char *p, unsigned len, unsigned pos)
{
  gxString why, feeds;
  gxList<ldm_prod_spec> specs;
  if(read_prod_class(p, len, pos, specs) != 0) {
    why << clear << "ALLOW DENIED - bad product class in request";
    return deny(s, why);
  }

  gxListNode<ldm_prod_spec> *sp = specs.GetHead();
  while(sp) {
    unsigned int requested = sp->data.feedtype;
    unsigned int covered = 0;
    set_ldm_feed_type_strings(requested, feeds);
    match_host_entries(s);
    gxListNode<LDMallow *> *ap = host_allow.GetHead();
    while(ap) {
      unsigned int allowed = 0;
      if(ldm_feed_expr_to_mask(ap->data->feedtype, allowed) == 0 && (allowed & requested) != 0) {
	covered |= (allowed & requested);
	// The request pattern must pass this entry's OK and NOT patterns
	if(!ap->data->OK_pattern.is_null() && compare_ldm_regex(ap->data->OK_pattern, sp->data.pattern) != 0) {
	  why << clear << "ALLOW DENIED - request pattern " << sp->data.pattern << " does not match OK pattern " << ap->data->OK_pattern;
	  return deny(s, why);
	}
	if(!ap->data->NOT_pattern.is_null() && compare_ldm_regex(ap->data->NOT_pattern, sp->data.pattern) == 0) {
	  why << clear << "ALLOW DENIED - request pattern " << sp->data.pattern << " matches NOT pattern " << ap->data->NOT_pattern;
	  return deny(s, why);
	}
      }
      ap = ap->next;
    }
    if(requested == 0 || covered != requested) {
      gxString missing;
      set_ldm_feed_type_strings(requested & ~covered, missing);
      why << clear << "ALLOW DENIED - " << (proc == FEEDME ? "FEEDME" : "NOTIFYME") << " for " << feeds 
	  << ", no ALLOW entry for " << missing;
      return deny(s, why);
    }
    if(servercfg->debug && servercfg->debug_level >= 2) {
      gxString message;
      message << clear << "[" << s->seq_num << "]: ALLOW GRANTED - " << feeds << " pattern " << sp->data.pattern;
      LogDebugMessage(message.c_str());
    }
    sp = sp->next;
  }
  return 0;
}

int LdmFilter::check_hiya(ClientSocket_t *s, const unsigned char *p, unsigned len, unsigned pos)
{
  gxString why, feeds;
  gxList<ldm_prod_spec> specs;
  if(read_prod_class(p, len, pos, specs) != 0) {
    why << clear << "ACCEPT DENIED - bad product class in HIYA";
    return deny(s, why);
  }
  gxListNode<ldm_prod_spec> *sp = specs.GetHead();
  while(sp) {
    unsigned int offered = sp->data.feedtype;
    unsigned int covered = 0;
    match_host_entries(s);
    gxListNode<LDMaccept *> *ap = host_accept.GetHead();
    while(ap) {
      unsigned int accepted = 0;
      if(ldm_feed_expr_to_mask(ap->data->feedtype, accepted) == 0) covered |= (accepted & offered);
      ap = ap->next;
    }
    set_ldm_feed_type_strings(offered, feeds);
    if(offered == 0 || covered != offered) {
      gxString missing;
      set_ldm_feed_type_strings(offered & ~covered, missing);
      why << clear << "ACCEPT DENIED - HIYA for " << feeds << ", no ACCEPT entry for " << missing;
      return deny(s, why);
    }
    sp = sp->next;
  }
  return 0;
}

int LdmFilter::check_product(ClientSocket_t *s, const char *what, const unsigned char *p, unsigned len, unsigned pos)
// Check a prod_info: the feed type and identifier must match an ACCEPT entry
{
  gxString why, origin, ident, feeds;
  unsigned int feedtype, seqno;
  if(xdr_skip(len, pos, 8 + 16) != 0 ||                 // arrival time and signature
     xdr_string(p, len, pos, 255, origin) != 0 ||
     xdr_u32(p, len, pos, feedtype) != 0 ||
     xdr_u32(p, len, pos, seqno) != 0 ||
     xdr_string(p, len, pos, 255, ident) != 0) {
    why << clear << "ACCEPT PROD DENIED - bad " << what << " product information";
    return deny(s, why);
  }
  set_ldm_feed_type_strings(feedtype, feeds);
  match_host_entries(s);
  gxListNode<LDMaccept *> *ap = host_accept.GetHead();
  while(ap) {
    unsigned int accepted = 0;
    if(ldm_feed_expr_to_mask(ap->data->feedtype, accepted) == 0 && 
       feedtype != 0 && (feedtype & ~accepted) == 0) {
      if(ap->data->prodIdEre.is_null() || compare_ldm_regex(ap->data->prodIdEre, ident) == 0) {
	if(servercfg->debug && servercfg->debug_level >= 2) {
	  gxString message;
	  message << clear << "[" << s->seq_num << "]: ACCEPT PROD GRANTED - " << what << " " << feeds << " " << ident;
	  LogDebugMessage(message.c_str());
	}
	return 0;
      }
    }
    ap = ap->next;
  }
  why << clear << "ACCEPT PROD DENIED - " << what << " " << feeds << " product " << ident << " from origin " << origin
      << " matches no ACCEPT entry";
  return deny(s, why);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
