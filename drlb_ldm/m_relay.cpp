// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- //
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 10/25/2026
// Date Last Modified: 10/25/2026
// Copyright (c) 2016-2026 DataReel Software Development
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

Frontend to backend data relay used by all DRLB servers.

10/25/2026: Replaces the LB_ReadWrite() and LB_CachedReadWrite()
loops. Those loops polled both sockets with non-blocking MSG_PEEK
calls (a busy loop on idle connections), dropped the rest of a short
write, did not limit how much one side could buffer, and with TLS
missed data OpenSSL had already decrypted.
*/
// ----------------------------------------------------------- //
#include "gxdlcode.h"

#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "drlb_server.h"
#include "m_relay.h"

void RelayBuffer::Cat(const void *p, unsigned bytes)
{
  if(bytes == 0) return;
  if(len + bytes > cap) {
    unsigned ncap = cap ? cap : 4096;
    while(ncap < len + bytes) ncap *= 2;
    char *nbuf = new char[ncap];
    if(len) memcpy(nbuf, buf, len);
    delete[] buf;
    buf = nbuf;
    cap = ncap;
  }
  memcpy(buf + len, p, bytes);
  len += bytes;
}

void RelayBuffer::DeleteFront(unsigned bytes)
{
  if(bytes >= len) { len = 0; return; }
  memmove(buf, buf + bytes, len - bytes);
  len -= bytes;
}

void SetSocketTimeouts(int fd, int send_secs, int recv_secs)
{
  struct timeval tv;
  if(send_secs >= 0) {
    tv.tv_sec = send_secs; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
  }
  if(recv_secs >= 0) {
    tv.tv_sec = recv_secs; tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
  }
}

static int set_nonblocking(int fd, int on)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if(flags < 0) return -1;
  if(on) flags |= O_NONBLOCK; else flags &= ~O_NONBLOCK;
  return fcntl(fd, F_SETFL, flags);
}

static int wait_for(int fd, short events, int timeout_ms)
// Returns > 0 if ready, 0 on timeout, < 0 on error
{
  struct pollfd p;
  p.fd = fd; p.events = events; p.revents = 0;
  int rv;
  do {
    rv = poll(&p, 1, timeout_ms);
  } while(rv < 0 && errno == EINTR);
  return rv;
}

// Read up to bytes from one side.
// Returns > 0 bytes read, 0 if the peer closed, -1 on error, -2 if no data is ready.
static int side_read(RelaySide &side, char *buf, int bytes)
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(side.ssl) {
    ERR_clear_error();
    int rv = SSL_read(side.ssl, buf, bytes);
    if(rv > 0) return rv;
    switch(SSL_get_error(side.ssl, rv)) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE: // TLS 1.3 tickets and key updates
	return -2;
      case SSL_ERROR_ZERO_RETURN:
	return 0;
      case SSL_ERROR_SYSCALL:
	if(ERR_peek_error() == 0 && errno == 0) return 0; // EOF without close_notify
	return -1;
      default:
	return -1;
    }
  }
#endif
  int rv;
  do {
    rv = recv(side.fd, buf, bytes, MSG_DONTWAIT);
  } while(rv < 0 && errno == EINTR);
  if(rv >= 0) return rv;
  if(errno == EAGAIN || errno == EWOULDBLOCK) return -2;
  return -1;
}

// True if a read on this side will return data without waiting.
static int side_has_data(RelaySide &side)
{
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(side.ssl && SSL_pending(side.ssl) > 0) return 1;
#endif
  return wait_for(side.fd, POLLIN, 0) > 0;
}

int RelayWriteAll(RelaySide &side, const char *buf, int bytes, int timeout_secs)
{
  int timeout_ms = (timeout_secs > 0) ? timeout_secs * 1000 : -1;
  int done = 0;
  while(done < bytes) {
#if defined (__USE_GX_SSL_EXTENSIONS__)
    if(side.ssl) {
      ERR_clear_error();
      int rv = SSL_write(side.ssl, buf + done, bytes - done);
      if(rv > 0) { done += rv; continue; }
      int err = SSL_get_error(side.ssl, rv);
      short ev;
      if(err == SSL_ERROR_WANT_WRITE) ev = POLLOUT;
      else if(err == SSL_ERROR_WANT_READ) ev = POLLIN;
      else return -1;
      if(wait_for(side.fd, ev, timeout_ms) <= 0) return -1; // Timeout or error
      continue;
    }
#endif
    int rv = send(side.fd, buf + done, bytes - done, MSG_DONTWAIT | MSG_NOSIGNAL);
    if(rv > 0) { done += rv; continue; }
    if(rv < 0 && errno == EINTR) continue;
    if(rv < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      if(wait_for(side.fd, POLLOUT, timeout_ms) <= 0) return -1; // Timeout or error
      continue;
    }
    return -1;
  }
  return 0;
}

// Move one burst of data from src to dst.
// Returns 0 to keep relaying, 1 if src closed, -1 on errors.
static int relay_burst(ClientSocket_t *s, RelaySide &src, RelaySide &dst, char *buffer,
		       int buffer_size, RelayFilter *filter, RelayBuffer &out)
{
  gxString message;
  unsigned long burst = 0;
  unsigned long max_burst = (unsigned long)servercfg->max_cache_size;
  int closed = 0;
  int rv = 0;

  out.Clear();
  while(1) {
    int br = side_read(src, buffer, buffer_size);
    if(br == -2) break; // Nothing more ready
    if(br == 0) { closed = 1; break; }
    if(br < 0) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << s->seq_num << "]: Error reading from " << src.name << " socket";
	LogMessage(message.c_str());
      }
      rv = -1;
      break;
    }
    burst += br;
    if(filter) {
      if(filter->Filter(s, src.is_frontend, buffer, br, out) != 0) {
	rv = -1;
	break;
      }
    }
    else {
      out.Cat(buffer, br);
    }
    if(src.is_frontend && servercfg->enable_buffer_overflow_detection &&
       burst >= (unsigned long)servercfg->buffer_overflow_size) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << s->seq_num << "]: Frontend read " << burst << " bytes from client";
	LogMessage(message.c_str());
	message << clear << "[" << s->seq_num << "]: Closing Frontend connection due to buffer overflow";
	LogMessage(message.c_str());
      }
      return -1;
    }
    // Without the buffer cache each read is written before the next one
    if(!servercfg->use_buffer_cache) break;
    if(burst >= max_burst) break;
    if(!side_has_data(src)) break;
  }

  if(out.length() > 0 && rv == 0) {
    if(RelayWriteAll(dst, out.m_buf(), out.length(), servercfg->write_timeout_secs) != 0) {
      if(servercfg->log_level >= 3) {
	message << clear << "[" << s->seq_num << "]: Error writing " << out.length() << " bytes to " << dst.name << " socket";
	LogMessage(message.c_str());
      }
      return -1;
    }
    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear << "[" << s->seq_num << "]: Moved " << out.length() << " bytes from " << src.name << " to " << dst.name;
      LogDebugMessage(message.c_str());
    }
  }
  out.Clear();

  if(rv != 0) return rv;
  if(closed) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear << "[" << s->seq_num << "]: " << src.name << " socket disconnected";
      LogDebugMessage(message.c_str());
    }
    return 1;
  }
  return 0;
}

int LB_Relay(ClientSocket_t *s, RelaySide &frontend, RelaySide &backend, int buffer_size, RelayFilter *filter)
{
  gxString message;
  if(buffer_size <= 0) buffer_size = 1500;
  char *buffer = new char[buffer_size];
  RelayBuffer out;
  int error_level = 0;

  set_nonblocking(frontend.fd, 1);
  set_nonblocking(backend.fd, 1);

  // Idle timeout in milliseconds, 0 for no timeout
  long idle_limit_ms = 0;
  if(servercfg->use_timeout) {
    idle_limit_ms = (long)servercfg->timeout_secs * 1000 + servercfg->timeout_usecs / 1000;
  }
  const int slice_ms = 1000; // Wake up to check for server shutdown
  long idle_ms = 0;

  while(servercfg->accept_clients) {
    int f_ready = side_has_data(frontend);
    int b_ready = side_has_data(backend);
    if(!f_ready && !b_ready) {
      struct pollfd p[2];
      p[0].fd = frontend.fd; p[0].events = POLLIN; p[0].revents = 0;
      p[1].fd = backend.fd; p[1].events = POLLIN; p[1].revents = 0;
      int wait_ms = slice_ms;
      if(idle_limit_ms > 0 && (idle_limit_ms - idle_ms) < wait_ms) wait_ms = (int)(idle_limit_ms - idle_ms);
      if(wait_ms < 0) wait_ms = 0;
      int rv = poll(p, 2, wait_ms);
      if(rv < 0) {
	if(errno == EINTR) continue;
	error_level = 1;
	break;
      }
      if(rv == 0) {
	idle_ms += wait_ms;
	if(idle_limit_ms > 0 && idle_ms >= idle_limit_ms) {
	  if(servercfg->debug || servercfg->log_level >= 4) {
	    message << clear << "[" << s->seq_num << "]: Connection idle timeout after "
		    << servercfg->timeout_secs << " secs " << servercfg->timeout_usecs << " usecs";
	    LogMessage(message.c_str());
	  }
	  error_level = 1;
	  break;
	}
	continue;
      }
      f_ready = (p[0].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
      b_ready = (p[1].revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }
    idle_ms = 0;

    int rv = 0;
    if(f_ready) rv = relay_burst(s, frontend, backend, buffer, buffer_size, filter, out);
    if(rv == 0 && b_ready) rv = relay_burst(s, backend, frontend, buffer, buffer_size, filter, out);
    if(rv != 0) {
      if(rv < 0) error_level = 1;
      break;
    }
  }

  delete[] buffer;
  return error_level;
}

HttpForwardFilter::HttpForwardFilter()
{
  state = HTTP_HEADER;
  remaining = 0;
}

int HttpForwardFilter::Filter(ClientSocket_t *s, int from_frontend, const char *buf, int bytes, RelayBuffer &out)
{
  if(!from_frontend || state == HTTP_PASSTHRU) {
    out.Cat(buf, bytes);
    return 0;
  }

  int pos = 0;
  while(pos < bytes) {
    if(state == HTTP_PASSTHRU) {
      out.Cat(buf + pos, bytes - pos);
      return 0;
    }
    if(state == HTTP_HEADER) {
      // Collect the header up to and including the blank line
      const int max_header = 65536;
      while(pos < bytes) {
	header.Cat(buf[pos++]);
	unsigned hl = header.length();
	if(hl >= 4 && memcmp(header.m_buf() + hl - 4, "\r\n\r\n", 4) == 0) break;
	if(hl >= (unsigned)max_header) { // Not a request we can parse, stop inspecting
	  out.Cat(header.m_buf(), header.length());
	  header.Clear();
	  state = HTTP_PASSTHRU;
	  break;
	}
      }
      unsigned hl = header.length();
      if(state == HTTP_HEADER && hl >= 4 && memcmp(header.m_buf() + hl - 4, "\r\n\r\n", 4) == 0) {
	if(process_header(s, out) != 0) return 1;
      }
      continue;
    }
    int used = 0;
    if(process_body(buf + pos, bytes - pos, used, out) != 0) return 1;
    pos += used;
  }
  return 0;
}

static int header_name_is(const char *line, int len, const char *name)
{
  int nlen = strlen(name);
  if(len <= nlen || line[nlen] != ':') return 0;
  return strncasecmp(line, name, nlen) == 0;
}

int HttpForwardFilter::process_header(ClientSocket_t *s, RelayBuffer &out)
// Rewrite one complete request header and set up the body state.
{
  const char *h = (const char *)header.m_buf();
  int hl = (int)header.length();

  // Only requests that start with a configured method are rewritten
  int is_request = 0;
  unsigned i;
  for(i = 0; i < servercfg->num_http_requests; i++) {
    int ml = servercfg->http_requests[i].length();
    if(ml > 0 && hl > ml && memcmp(h, servercfg->http_requests[i].c_str(), ml) == 0 && h[ml] == ' ') {
      is_request = 1;
      break;
    }
  }
  if(!is_request) {
    // Not HTTP/1.x, or a method we do not handle. Pass the rest of the
    // connection through unchanged.
    out.Cat(h, hl);
    header.Clear();
    state = HTTP_PASSTHRU;
    return 0;
  }

  unsigned long long content_length = 0;
  int chunked = 0;
  int upgrade = 0;
  int line_start = 0;
  int first = 1;
  while(line_start < hl - 2) { // Stop at the blank line
    int e = line_start;
    while(e < hl - 1 && !(h[e] == '\r' && h[e+1] == '\n')) e++;
    int len = e - line_start;
    const char *line = h + line_start;
    int keep = 1;
    if(!first) {
      if(header_name_is(line, len, servercfg->http_forward_for_str.c_str())) {
	keep = 0; // Remove forwarding headers sent by the client
      }
      else if(header_name_is(line, len, "Content-Length")) {
	gxString v; v.InsertAt(0, line + 15, len - 15);
	v.TrimLeadingSpaces(); v.TrimTrailingSpaces();
	content_length = strtoull(v.c_str(), 0, 10);
      }
      else if(header_name_is(line, len, "Transfer-Encoding")) {
	gxString v; v.InsertAt(0, line + 18, len - 18);
	if(v.IFind("chunked") != -1) chunked = 1;
      }
      else if(header_name_is(line, len, "Upgrade")) {
	upgrade = 1;
      }
    }
    if(keep) out.Cat(line, len + 2);
    first = 0;
    line_start = e + 2;
  }
  gxString ff;
  ff << clear << servercfg->http_forward_for_str << ": " << s->client_name << "\r\n\r\n";
  out.Cat((void *)ff.c_str(), ff.length());
  header.Clear();

  if(upgrade || strncmp(h, "CONNECT ", 8) == 0) {
    state = HTTP_PASSTHRU; // WebSocket or tunnel, no more HTTP requests follow
  }
  else if(chunked) {
    state = HTTP_CHUNK_SIZE;
    line.Clear();
  }
  else if(content_length > 0) {
    state = HTTP_BODY_LENGTH;
    remaining = content_length;
  }
  else {
    state = HTTP_HEADER;
  }
  return 0;
}

int HttpForwardFilter::process_body(const char *buf, int bytes, int &used, RelayBuffer &out)
// Pass request body bytes and find where the next request starts.
{
  used = 0;
  switch(state) {
    case HTTP_BODY_LENGTH:
    case HTTP_CHUNK_DATA: {
      unsigned long long n = remaining;
      if(n > (unsigned long long)bytes) n = bytes;
      out.Cat(buf, (unsigned)n);
      used = (int)n;
      remaining -= n;
      if(remaining == 0) state = (state == HTTP_BODY_LENGTH) ? HTTP_HEADER : HTTP_CHUNK_DATA_END;
      return 0;
    }
    case HTTP_CHUNK_SIZE:
    case HTTP_CHUNK_DATA_END:
    case HTTP_TRAILER:
      while(used < bytes) {
	char c = buf[used++];
	out.Cat(c);
	if(c != '\n') {
	  if(line.length() > 4096) { state = HTTP_PASSTHRU; return 0; }
	  if(c != '\r') line << c;
	  continue;
	}
	// One line complete
	if(state == HTTP_CHUNK_DATA_END) { // CRLF after chunk data
	  line.Clear();
	  state = HTTP_CHUNK_SIZE;
	  return 0;
	}
	if(state == HTTP_CHUNK_SIZE) {
	  remaining = strtoull(line.c_str(), 0, 16);
	  line.Clear();
	  state = (remaining == 0) ? HTTP_TRAILER : HTTP_CHUNK_DATA;
	  return 0;
	}
	// Trailer section ends with an empty line
	int empty = line.is_null();
	line.Clear();
	if(empty) state = HTTP_HEADER;
	return 0;
      }
      return 0;
    default:
      out.Cat(buf, bytes);
      used = bytes;
      return 0;
  }
}
// ----------------------------------------------------------- //
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
