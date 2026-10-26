// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Header File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 10/25/2026
// Date Last Modified: 10/25/2026
// Copyright (c) 2016-2026 DataReel Software Development
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

Frontend to backend data relay used by all DRLB servers. This file is
the same in the load-balancer, ssl-bridge and LDM-load-balancer
projects.

The relay waits on both sockets with poll(), so idle connections use
no CPU. Data read from one side is passed through an optional filter
and written in full to the other side. Writes and idle periods are
bounded by timeouts, and the amount of data read in one burst is
capped by max_cache_size.
*/
// ----------------------------------------------------------- //
#ifndef __DRLB_MRELAY_HPP__
#define __DRLB_MRELAY_HPP__

#include "gxdlcode.h"

#include "membuf.h"
#include "gxstring.h"
#include "gxssl.h"

struct ClientSocket_t;

// One side of a relayed connection: a plain TCP socket or a TLS session
// on top of one.
struct RelaySide {
  RelaySide() { fd = -1; name = ""; is_frontend = 0;
#if defined (__USE_GX_SSL_EXTENSIONS__)
    ssl = 0;
#endif
  }
  int fd;
  const char *name;
  int is_frontend;
#if defined (__USE_GX_SSL_EXTENSIONS__)
  SSL *ssl; // Null for plain TCP
#endif
};

// Filters see every block of data read from either side, in order.
// Filter() appends the bytes to forward to out, which may differ from
// the input (headers added or removed) or hold bytes back until more
// data arrives. Returns 0 to continue or non-zero to close the
// connection.
class RelayFilter
{
public:
  RelayFilter() { }
  virtual ~RelayFilter() { }

public:
  virtual int Filter(ClientSocket_t *s, int from_frontend, const char *buf, int bytes, MemoryBuffer &out) = 0;
};

// HTTP X-Forwarded-For filter. Adds one forwarding header with the
// client address to each request, after removing any forwarding header
// sent by the client. Requests with a Content-Length or a chunked body
// are tracked so every request on a keep alive connection is handled.
class HttpForwardFilter : public RelayFilter
{
public:
  HttpForwardFilter();
  ~HttpForwardFilter() { }

public:
  int Filter(ClientSocket_t *s, int from_frontend, const char *buf, int bytes, MemoryBuffer &out);

private:
  int process_header(ClientSocket_t *s, MemoryBuffer &out);
  int process_body(const char *buf, int bytes, int &used, MemoryBuffer &out);

private:
  enum { HTTP_HEADER, HTTP_BODY_LENGTH, HTTP_CHUNK_SIZE, HTTP_CHUNK_DATA,
	 HTTP_CHUNK_DATA_END, HTTP_TRAILER, HTTP_PASSTHRU };
  int state;
  MemoryBuffer header;          // Request header being collected
  unsigned long long remaining; // Body or chunk bytes left to pass
  gxString line;                // Chunk size or trailer line being collected
};

// Copy data between the frontend and backend until one side closes,
// an error or timeout occurs, or the filter ends the connection.
// Returns 0 when a side closed normally, non-zero for errors.
int LB_Relay(ClientSocket_t *s, RelaySide &frontend, RelaySide &backend,
	     int buffer_size, RelayFilter *filter = 0);

// Set the send and receive timeouts on a socket. A value of 0 clears it.
void SetSocketTimeouts(int fd, int send_secs, int recv_secs);

// Write all bytes to one side, waiting up to the write timeout.
// Returns 0 if all bytes were written.
int RelayWriteAll(RelaySide &side, const char *buf, int bytes, int timeout_secs);

#endif // __DRLB_MRELAY_HPP__
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
