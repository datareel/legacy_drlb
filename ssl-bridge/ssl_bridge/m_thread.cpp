// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 10/19/2016
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

Multi-threaded framework Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#include <regex.h>
#include <string.h>
#include <errno.h>

#include "gxstring.h"
#include "gxlist.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "m_thread.h"
#include "m_log.h"
#include "membuf.h"
#include "m_relay.h"

#include "ssl_utils.h"
#include "ssl_config.h"

ClientSocket_t::~ClientSocket_t()
{
  FreeSSL();
} 

void ClientSocket_t::FreeSSL() 
{
  if(openssl) {
    openssl->ReleaseCTX(); // Do not free Context by ~gxSSL() call
    delete openssl; 
    openssl = 0;
  }
  if(openssl_client) {
    openssl_client->ReleaseCTX(); // The shared backend context is freed at exit
    delete openssl_client;
    openssl_client = 0;
  }
}

int LBServerThread::InitServer(int max_connections)
// Initialize the server. Returns a non-zero value if any errors ocurr.
{

  if(LoadSSLConfig() != 0) {
    StopProc();
    ExitProc(1);
  }

  // Load the SSL key and cert
  if(init_ssl_cert() != 0) {
    StopProc();
    ExitProc(1);
  }

  if(InitSocketLibrary() == 0) {
    if(InitSocket(SOCK_STREAM, servercfg->port) < 0) return 1;
  }
  else {
    return 1;
  }

  int reuse = 1;
  if(setsockopt(gxsocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
    if(servercfg->debug) {
      LogMessage("ERROR - Socket error setting SO_REUSEADDR option");
    }
  }

  // 10/25/2026: SO_REUSEPORT is no longer set. It let a second server
  // bind the same port and silently take a share of the connections.

  if(!servercfg->listen_ip_addr.is_null()) {
    if(servercfg->listen_ip_addr != "0.0.0.0") {
      if(servercfg->listen_ip_addr != "::0") {
	inet_aton(servercfg->listen_ip_addr.c_str(), &sin.sin_addr);
      }
    }
  }

  // Bind the name to the socket
  if(Bind() < 0) {
    LogMessage("ERROR - Fatal bind() error initializing server");
    CheckSocketError((gxSocket *)this);
    Close();
    return 1;
  }
    
  // Listen for connections with a specified backlog
  if(Listen(max_connections) < 0) {
    LogMessage("ERROR - Fatal listen() error initializing server");
    CheckSocketError((gxSocket *)this);
    Close();
    return 1;
  }

  return 0;
}

void *LBServerThread::ThreadEntryRoutine(gxThread_t *thread)
{
  gxString message, sbuf;
  unsigned long accept_errors = 0;
  int error_number = 0;
  int num_start_threads = 4; // main + server_thread + log_thread + process_thread
  if(servercfg->enable_stats) num_start_threads++;
  int thread_count = 0;
  int num_connections;

  while(servercfg->accept_clients) { 
    if(servercfg->debug && servercfg->debug_level >= 5) {
      LogDebugMessage("LB server thread accept() call");
    }

    memset(&remote_sin, 0, sizeof(gxsSocketAddress));
    gxsSocketLength_t addr_size = (gxsSocketLength_t)sizeof(remote_sin); 
    remote_socket = accept(gxsocket, (struct sockaddr *)&remote_sin, &addr_size);
    if(remote_socket < 0) {
      // 10/25/2026: Only a broken listening socket ends the server. Running
      // out of file descriptors or memory, or a client resetting before
      // accept() returns, is transient. These errors used to stop the whole
      // balancer after 5 tries, so a connection flood could shut it down.
      get_fd_error(error_number, sbuf);
      if(error_number == EBADF || error_number == EINVAL || error_number == ENOTSOCK ||
	 error_number == EOPNOTSUPP || error_number == EFAULT) {
	message << clear << "ERROR - accept() call failed, ENUM: " << error_number << " Message: " << sbuf;
	LogMessage(message.c_str());
	break;
      }
      if(error_number == EINTR || error_number == ECONNABORTED || error_number == EAGAIN) continue;
      if(accept_errors++ == 0 || servercfg->debug) {
	message << clear << "ERROR - accept() call failed, will keep retrying, ENUM: " << error_number << " Message: " << sbuf;
	LogMessage(message.c_str());
      }
      mSleep(accept_errors < 10 ? 10 * accept_errors : 100); // Back off while resources are exhausted
      continue;
    }
    if(accept_errors > 0) {
      message << clear << "INFO - accept() recovered after " << accept_errors << " failed calls";
      LogMessage(message.c_str());
      accept_errors = 0;
    }

    servercfg->CONNECTION_TOTAL(1);

    char client_name[gxsMAX_NAME_LEN]; int r_port = -1;
    memset(client_name, 0, gxsMAX_NAME_LEN);
    GetClientInfo(client_name, r_port);
    // NOTE: gxSocket library was updated to use code below:
    // r_port = ntohs(remote_sin.sin_port);
    // inet_ntop(AF_INET, &remote_sin.sin_addr, client_name, gxsMAX_NAME_LEN);
    int seq_num = r_port;
    if(servercfg->debug || servercfg->log_level >= 1) {
      message << clear << "[" << seq_num << "]: Frontend client connect: " << client_name;
      LogMessage(message.c_str());
    }

    thread_count = 0;
    num_connections = 1; // Count this connection 
    thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
    while(ptr) {
      gxThread_t *thread = ptr->GetThreadPtr();
      if(thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
	thread_count++; // count all the running client threads
	num_connections++; // count all the connected threads
      }
      ptr = ptr->GetNext();
    }

    servercfg->num_client_threads = thread_count;
    int num_threads = num_start_threads + thread_count;
    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear << num_threads << " threads working";
      LogDebugMessage(message.c_str());
    }

    servercfg->NUM_SERVER_CONNECTIONS(num_connections);

    if(servercfg->debug && servercfg->debug_level >= 5) {
      message << clear <<  num_connections << " server connections";
      LogDebugMessage(message.c_str());
    }

    if(servercfg->max_threads > 1) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Max thread limit is set to " << servercfg->max_threads; 
	LogDebugMessage(message.c_str());
      }
      if(num_threads >= servercfg->max_threads) {
	message << clear << "[" << seq_num << "]: Reached " << num_threads << " max thread limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(remote_socket); remote_socket = -1;
	continue;
      }
    }

    if(servercfg->max_connections > 1) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	message << clear << "Max frontend connection limit is set to " << servercfg->max_connections; 
	LogDebugMessage(message.c_str());
      }
      if(num_connections >= servercfg->max_connections) {
	message << clear << "[" << seq_num << "]: Reached " << num_connections
		<< " max frontend connection limit. " << client_name << " connection denied";
	LogMessage(message.c_str());
	close(remote_socket); remote_socket = -1;
	continue;
      }
    }

    ClientSocket_t *s = new ClientSocket_t;
    if(!s) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Fatal memory error ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      servercfg->accept_clients = 0;
      break;
    }
    
    // Record the file descriptor assigned by the Operating System
    s->client_socket = remote_socket;
    s->seq_num = seq_num;
    s->client_name = client_name;
    s->r_port = r_port;

    s->openssl = new gxSSL(sslcfg->openssl.GetCTX());
    if(!s->openssl) {
      message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
      LogMessage(message.c_str());
      servercfg->accept_clients = 0;
      delete s;
      break;
    }
    if(sslcfg->ssl_encrypt_backend) {
      // 10/25/2026: One client context is shared by every backend connection
      s->openssl_client = new gxSSL(sslcfg->backend.GetCTX());
      if(!s->openssl_client) {
	message << clear << "[" << seq_num << "]: ERROR - A fatal memory allocation error occurred in main server thread";
	LogMessage(message.c_str());
	servercfg->accept_clients = 0;
	delete s;
	break;
      }
    }

    if(servercfg->debug && servercfg->debug_level >= 5) LogDebugMessage("LB server rebuilding thread pool");
    RebuildThreadPool(servercfg->client_request_pool);

    if(servercfg->debug && servercfg->debug_level >= 5) LogDebugMessage("LB server starting client request thread");

    // 08/18/2016: Must set thread type to detached to free thread resources. To watch memory usage under high load:
    // $ watch -n 1 'ps -eo %mem,pid,user,args | grep drlb | grep -v grep'
    // $ watch -n 1 "cat /proc/$(ps -ef | grep drlb_server | grep -v grep | awk '{ print $2 }')/status | grep -i vmsize"
    //
    gxThreadType type = gxTHREAD_TYPE_DETACHED;
    gxThreadPriority prio = gxTHREAD_PRIORITY_NORMAL;
    gxStackSizeType ssize = 2048;
    gxThread_t *rthread = request_thread.CreateThread(servercfg->client_request_pool, (void *)s, prio, type, ssize);
    if(!rthread) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - Memory allocation main server thread, cannot allocate client thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: Fatal thread create error ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      delete s; s = 0;
      break;
    }

    if(CheckThreadError(rthread, seq_num)) {
      get_fd_error(error_number, sbuf);
      message << clear << "[" << seq_num << "]: ERROR - Fatal error starting client thread";
      LogMessage(message.c_str());
      message << clear << "[" << seq_num << "]: " << rthread->ThreadExceptionMessage() << " ENUM: " << error_number << " Message: " << sbuf;
      LogProcMessage(message.c_str());
      delete s; s = 0;
      break;
    }
  }
  servercfg->accept_clients = 0;

  // Send notice that program is ending
  NT_print("NOTICE - Main server thread has exited");
  
  if(servercfg->debug && servercfg->debug_level >= 5) { 
    LogDebugMessage("Server thread exit routine closing connections");
  }
  CloseSocket(); // Close the server side socket

  // Fatal error, we need to tell the process thread to exit the process
  servercfg->kill_server = 1;
  servercfg->process_cond.ConditionSignal(); 
  return (void *)0;
}

void LBServerThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  servercfg->accept_clients = 0;

  if(servercfg->debug && servercfg->debug_level > 5) { // We should never log from exit/cancel call 
    LogDebugMessage("Server thread clean up handler closing connections");
  }

  CloseSocket(); // Close the server side socket
}

void *LBClientRequestThread::ThreadEntryRoutine(gxThread_t *thread)
{
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
  gxString message;

  if(s) {
    if(servercfg->enable_throttling) {
      int throttle_this_connection = 0;
      int throttle_count = servercfg->THROTTLE_COUNT(1);
      if(throttle_count == 0) {
	if(servercfg->throttle_apply_by_load) {
	  if(servercfg->CONNECTIONS_PER_SECOND() >= servercfg->throttle_connections_per_sec) {
	    throttle_this_connection = 1;
	  }
	} 
	else {
	  throttle_this_connection = 1;
	}
	if(throttle_this_connection) {
	  if(servercfg->debug && servercfg->debug_level >= 5) { 
	    message << clear << "[" << s->seq_num  << "]: Throttling connection";
	    LogDebugMessage(message.c_str());
	  }
	  if(servercfg->throttle_wait_secs >= 1) sSleep(servercfg->throttle_wait_secs);
	  if(servercfg->throttle_wait_usecs >= 1) uSleep(servercfg->throttle_wait_usecs);
	}
      }
    }

    HandleClientRequest(s);

    if(servercfg->debug && servercfg->debug_level >= 5) { 
      message << clear << "[" << s->seq_num 
	      << "]: Client request thread exit routine closing connections";
      LogDebugMessage(message.c_str());
    }

    if(s->node) s->node->NUM_CONNECTIONS(0, 1);
    s->openssl->CloseSSLSocket();
    if(sslcfg->ssl_encrypt_backend && s->openssl_client) s->openssl_client->CloseSSLSocket();
    if(s->client_socket > 0) { close(s->client_socket); s->client_socket = -1; }
    if(s->client.GetSocket() > 0) { close(s->client.GetSocket()); s->client.SetSocket(-1); }
    delete s; s = 0;
  }

  return (void *)0;
}

void LBClientRequestThread::ThreadCleanupHandler(gxThread_t *thread)
// Thread cleanup handler used in the event that the thread is
// canceled.
{
  gxString message;
  ClientSocket_t *s = (ClientSocket_t *)thread->GetThreadParm();
  if(s) {
    if(servercfg->debug && servercfg->debug_level > 5) { // We should never log from exit/cancel call  
      message << clear << "[" << s->seq_num << "]: Client request thread exit routine closing connections";
      LogDebugMessage(message.c_str());
    }
    if(s->node) s->node->NUM_CONNECTIONS(0, 1);
    s->openssl->CloseSSLSocket();
    if(sslcfg->ssl_encrypt_backend && s->openssl_client) s->openssl_client->CloseSSLSocket();
    if(s->client_socket > 0) { close(s->client_socket); s->client_socket = -1; }
    if(s->client.GetSocket() > 0) { close(s->client.GetSocket()); s->client.SetSocket(-1); }
    delete s; s = 0;
  }
}

void SetupBackendSSL(ClientSocket_t *s, LBnode *n)
// 10/25/2026: Set the name sent as SNI and checked against the backend
// certificate: ssl_backend_hostname if set, or the node host name or IP.
// With ssl_verify_backend_cert the handshake fails if the certificate
// chain or name does not verify.
{
  if(!s->openssl_client || !n) return;
  if(!sslcfg->ssl_backend_hostname.is_null()) {
    s->openssl_client->SetHostName(sslcfg->ssl_backend_hostname.c_str());
  }
  else {
    s->openssl_client->SetHostName(n->hostname.c_str());
  }
  if(sslcfg->ssl_verify_backend_cert) {
    s->openssl_client->RequireVerifyCert();
  }
  else {
    s->openssl_client->NoVerifyCert();
  }
  SetSocketTimeouts(s->client.GetSocket(), sslcfg->ssl_handshake_timeout_secs, sslcfg->ssl_handshake_timeout_secs);
  // 11/15/2026: DataReel 6.4 sets its own handshake timeout (10 s by
  // default) on the socket during the handshake; use the configured one
  s->openssl_client->SetHandshakeTimeout(sslcfg->ssl_handshake_timeout_secs);
}

int get_fd_error(int &error_number, gxString &sbuf)
{
  error_number = errno;
  return fd_error_to_string(error_number, sbuf);
}

void LBClientRequestThread::HandleClientRequest(ClientSocket_t *s)
// Function used to handle a client request.
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  LBnode *n;
  
  // 10/25/2026: Bound the TLS handshake so a client that never finishes
  // it cannot hold this thread
  SetSocketTimeouts(s->client_socket, sslcfg->ssl_handshake_timeout_secs, sslcfg->ssl_handshake_timeout_secs);
  // 11/15/2026: DataReel 6.4 sets its own handshake timeout (10 s by
  // default) on the socket during the handshake; use the configured one
  s->openssl->SetHandshakeTimeout(sslcfg->ssl_handshake_timeout_secs);
  if(s->openssl->OpenSSLSocket(s->client_socket) != gxSSL_NO_ERROR) {
    if(servercfg->log_level >= 3) {
      sbuf << clear << "[" << seq_num << "]: Error opening frontend SSL socket";
      LogMessage(sbuf.c_str());
      sbuf << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
      LogMessage(sbuf.c_str());
    }
    return;
  }
  
  if(s->openssl->AcceptSSLSocket() != gxSSL_NO_ERROR) {
    if(servercfg->log_level >= 3) {
      sbuf << clear << "[" << seq_num << "]: Error accepting frontend SSL socket";
      LogMessage(sbuf.c_str());
      sbuf << clear << "[" << seq_num << "]: " << s->openssl->SSLErrorMessage();
      LogMessage(sbuf.c_str());
      gxString err;
      gxSSL::GlobalSSLError(err);
      sbuf << clear << "[" << seq_num << "]: " << err;
      LogMessage(sbuf.c_str());
    }
    return;
  }

  switch (servercfg->scheme) {
    case LB_ASSIGNED:
      n = assigned(s);
      if(!n) {
	switch(servercfg->assigned_default) {
	  case LB_RR:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will default to round robin";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = round_robin(s);
	    break;
	  case LB_DISTRIB:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to distributed";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = distrib(s);
	    break;
	  case LB_WEIGHTED:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will fail over to weighted";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = weighted(s);
	    break;
	  case LB_NONE:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will not fail over to another node";
	      LogDebugMessage(sbuf.c_str());
	    }
	    return;
	  default:
	    if(servercfg->debug && servercfg->debug_level >= 5) {
	      sbuf << clear << "[" << seq_num << "]: LB assigned will default to round robin";
	      LogDebugMessage(sbuf.c_str());
	    }
	    n = round_robin(s);
	    break;
	}
      }
      break;
    case LB_DISTRIB:
      n = distrib(s);
      break;
    case LB_RR:
      n = round_robin(s);
      break;
    case LB_WEIGHTED:
      n = weighted(s);
      break;
    default:
      n = round_robin(s);
      break;
  }

  if(!n) return; // No backend nodes ready

  s->node = n;
  n->NUM_CONNECTIONS(1);
  n->CONNECTION_TOTAL(1);

  if(servercfg->debug && servercfg->debug_level >= 5) {
    int num_connections = n->NUM_CONNECTIONS();
    message << clear << "[" << seq_num << "]: " << n->node_name << " has " << num_connections << " client connections";
    LogDebugMessage(message.c_str());
  }

  RelaySide frontend, backend;
  frontend.fd = s->client_socket; frontend.name = "Frontend"; frontend.is_frontend = 1;
  backend.fd = s->client.GetSocket(); backend.name = "Backend";
  frontend.ssl = s->openssl->GetSSL();
  if(sslcfg->ssl_encrypt_backend && s->openssl_client) backend.ssl = s->openssl_client->GetSSL();
  HttpForwardFilter http_filter;
  RelayFilter *filter = 0;
  if(servercfg->enable_http_forwarding) filter = &http_filter;
  LB_Relay(s, frontend, backend, n->buffer_size, filter);

  // 10/25/2026: Send TLS close_notify before closing the sockets. Without
  // it OpenSSL 3 clients report "unexpected eof while reading" and treat
  // the response as truncated.
  if(frontend.ssl) SSL_shutdown(frontend.ssl);
  if(backend.ssl) SSL_shutdown(backend.ssl);

  shutdown(s->client_socket, SHUT_RDWR);
  shutdown(s->client.GetSocket(), SHUT_RDWR);

  if(servercfg->debug && servercfg->debug_level >= 5) {
    message << clear << "[" << seq_num << "]: Client has disconnected, exiting client request thread";
    LogDebugMessage(message.c_str());
  }

  return;
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s)
{
  return round_robin(s, servercfg->nodes);
}

int LBClientRequestThread::connect_backend(ClientSocket_t *s, LBnode *n, const char *how)
// 10/25/2026: Connect to one backend node. Returns 0 if connected.
{
  gxString sbuf;
  unsigned seq_num = s->seq_num;
  gxSocket *client = &s->client;

  if(servercfg->log_level >= 2) {
    sbuf << clear << "[" << seq_num << "]: " << n->node_name << " " << how << " connect to " << n->hostname 
	 << " port " << n->port_number;
    LogMessage(sbuf.c_str());
  }
  client->Clear();
  if(client->InitSocket(SOCK_STREAM, n->port_number, n->hostname.c_str()) < 0) {
    if(servercfg->log_level >= 1) {
      sbuf << clear << "[" << seq_num << "]: ERROR - " << n->node_name << " cannot open connection to backend host " << n->hostname 
	   << " port " << n->port_number;
      LogMessage(sbuf.c_str());
      CheckSocketError(client, seq_num);
    }
    if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
    return 1;
  }
  SetSocketTimeouts(client->GetSocket(), servercfg->connect_timeout_secs, -1);
  if(client->Connect() < 0) {
    if(servercfg->log_level >= 1) {
      sbuf << clear << "[" << seq_num << "]: ERROR - " << n->node_name << " cannot connect to backend host " 
	   << n->hostname << " port " << n->port_number;
      LogMessage(sbuf.c_str());
      CheckSocketError(client, seq_num);
    }
    if(client->GetSocket() > 0) { client->Close(); client->SetSocket(-1); }
    return 1;
  }
#if defined (__USE_GX_SSL_EXTENSIONS__)
  if(sslcfg->ssl_encrypt_backend && s->openssl_client) {
    SetupBackendSSL(s, n);
    if(s->openssl_client->OpenSSLSocket(client->GetSocket()) != gxSSL_NO_ERROR) {
      if(servercfg->log_level >= 3) {
	sbuf << clear << "[" << seq_num << "]: Error opening backend SSL socket";
	LogMessage(sbuf.c_str());
	sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
	LogMessage(sbuf.c_str());
      }
      client->Close(); client->SetSocket(-1);
      return 1;
    }
    if(s->openssl_client->ConnectSSLSocket() != gxSSL_NO_ERROR) {
      sbuf << clear << "[" << seq_num << "]: Error connecting to backend SSL socket, certificate check: "
	   << gxSSL::CertVerifyErrorString(SSL_get_verify_result(s->openssl_client->GetSSL()));
      LogMessage(sbuf.c_str());
      sbuf << clear << "[" << seq_num << "]: " << s->openssl_client->SSLErrorMessage();
      LogMessage(sbuf.c_str());
      s->openssl_client->CloseSSLSocket();
      client->Close(); client->SetSocket(-1);
      return 1;
    }
  }
#endif
  return 0;
}

LBnode *LBClientRequestThread::round_robin(ClientSocket_t *s,  gxList<LBnode *> &node_list)
// 10/25/2026: Each connection starts at the next node in the list, taken
// from a shared counter, and tries every node once per pass. A failed
// pass is retried up to servercfg->retries times. The old version kept
// a used flag per node that was only reset by a retry, so with
// retries = 0 every other connection failed, and its retries restarted
// at the full node list instead of the list it was given.
{
  gxString sbuf;
  unsigned seq_num = s->seq_num;
  unsigned num_nodes = 0;
  gxListNode<LBnode *> *ptr = node_list.GetHead();
  while(ptr) { num_nodes++; ptr = ptr->next; }
  if(num_nodes == 0) return 0;

  unsigned long *counter = &servercfg->rr_counter[0];
  if(&node_list == &servercfg->distributed_rr_node_list) counter = &servercfg->rr_counter[1];
  if(&node_list == &servercfg->weighted_rr_node_list) counter = &servercfg->rr_counter[2];

  // 10/25/2026: Nodes that refused a connection are skipped for
  // dead_node_secs, and the start node is picked among the live nodes,
  // so a dead node's share is spread over all live nodes instead of
  // going to the node after it.
  LBnode **live = new LBnode *[num_nodes];
  unsigned retries = servercfg->retries;
  while(1) {
    time_t now = time(0);
    unsigned num_live = 0;
    for(ptr = node_list.GetHead(); ptr; ptr = ptr->next) {
      if(ptr->data->down_until <= now) live[num_live++] = ptr->data;
    }
    if(num_live == 0) { // All nodes are marked down, try them all
      for(ptr = node_list.GetHead(); ptr; ptr = ptr->next) live[num_live++] = ptr->data;
    }
    unsigned start = (unsigned)(__atomic_fetch_add(counter, 1UL, __ATOMIC_RELAXED) % num_live);
    for(unsigned tried = 0; tried < num_live; tried++) {
      LBnode *n = live[(start + tried) % num_live];
      if(servercfg->check_node_max_clients(n, seq_num) != 0) continue;
      if(connect_backend(s, n, "RR") == 0) {
	n->down_until = 0;
	delete[] live;
	return n;
      }
      if(servercfg->dead_node_secs > 0) n->down_until = time(0) + servercfg->dead_node_secs;
    }
    if(retries == 0) break;
    retries--;
    sbuf << clear << "[" << seq_num << "]: ERROR - All connection attempts failed, retries left " << retries;
    LogMessage(sbuf.c_str());
    if(servercfg->retry_wait > 0) sSleep(servercfg->retry_wait);
  }
  delete[] live;

  sbuf << clear << "[" << seq_num << "]: ERROR - All connection attempts failed";
  LogMessage(sbuf.c_str());
  return 0;
}

LBnode *LBClientRequestThread::assigned(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  unsigned error_level = 0;
  gxString client_name = s->client_name;
  regex_t regex;
  int reti;
  char mbuf[255];
  gxListNode<LB_rule *> *lptr = servercfg->rules_config_list.GetHead();
  error_level = 0;

  while(lptr) {
    if(lptr->data->rule == LB_ROUTE) {
      reti = regcomp(&regex, lptr->data->front_end_client_ip.c_str(), REG_EXTENDED|REG_NOSUB);
      if(reti != 0) { // This should have been checked in config read
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Bad regular expression in rules file: " << lptr->data->front_end_client_ip.c_str();
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	reti = regexec(&regex, s->client_name.c_str(), 0, NULL, 0);
	regfree(&regex);
	if(reti == 0) {
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP matches regex: " << s->client_name << " match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	  break;
	}
	else if(reti == REG_NOMATCH && servercfg->resolve_assigned_hostnames) {
	  gxString hostname_str, error_message;
	  if(ip_to_hostname(s->client_name, hostname_str, error_message) == 0) {
	    if((s->client_name != hostname_str) && (!hostname_str.is_null())) { 
	      reti = regcomp(&regex, lptr->data->front_end_client_ip.c_str(), REG_EXTENDED|REG_NOSUB|REG_ICASE);
	      if(reti == 0) {
		reti = regexec(&regex, hostname_str.c_str(), 0, NULL, 0);
		regfree(&regex);
	      }
	      else {
		reti = REG_NOMATCH; // 10/25/2026: Was used without checking regcomp()
	      }
	      if(reti == 0) {
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  sbuf << clear << "[" << seq_num << "]: INFO - Frontend hostname matches regex: " << hostname_str << " match " << lptr->data->front_end_client_ip;
		  LogDebugMessage(sbuf.c_str());
		}
		error_level = 0;
		break;
	      }
	      else {
		if(servercfg->debug && servercfg->debug_level >= 5) {
		  sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP/hostname does not match regex: " 
		       << s->client_name << "/" << hostname_str << " no match " << lptr->data->front_end_client_ip;
		  LogDebugMessage(sbuf.c_str());
		}
		error_level = 0;
	      }
	    }
	    else {
	      if(servercfg->debug && servercfg->debug_level >= 5) {
		sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP does not match regex: " << s->client_name << " no match " << lptr->data->front_end_client_ip;
		LogDebugMessage(sbuf.c_str());
	      }
	      error_level = 0;
	    }
	  }
	  else {
	    if(servercfg->debug && servercfg->debug_level > 3) {
	      sbuf << clear << "[" << seq_num << "]: ERROR - Get hostname error for IP " << s->client_name << " " << error_message;
	      LogDebugMessage(sbuf.c_str());
	    }
	    error_level = 0;
	  }
	}
	else if(reti == REG_NOMATCH) {
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: INFO - Frontend IP does not match regex: " << s->client_name << " no match " << lptr->data->front_end_client_ip;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 0;
	}
	else {
	  regerror(reti, &regex, mbuf, sizeof(mbuf));
	  if(servercfg->debug && servercfg->debug_level >= 5) {
	    sbuf << clear << "[" << seq_num << "]: ERROR - Regex match failed: " << mbuf;
	    LogDebugMessage(sbuf.c_str());
	  }
	  error_level = 1;
	}
      }
    }
    lptr = lptr->next;
  }

  if(!lptr) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      if(error_level != 0) {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: ERROR - Parsing errors reading " << servercfg->rules_config_file;
	  LogDebugMessage(sbuf.c_str());
	}
      }
      else {
	if(servercfg->debug && servercfg->debug_level >= 5) {
	  sbuf << clear << "[" << seq_num << "]: INFO - No matching rules in config file " << servercfg->rules_config_file;
	  LogDebugMessage(sbuf.c_str());
	}
      }
    }
    return 0;
  }
  
  gxListNode<LBnode *> *ptr = servercfg->nodes.GetHead();
  while(ptr) {
    if(ptr->data->node_name == lptr->data->node_name) {
      if(servercfg->debug && servercfg->debug_level >= 5) {
	sbuf << clear << "[" << seq_num << "]: Assigned node " << ptr->data->node_name << " matches rule " << lptr->data->front_end_client_ip;
	LogDebugMessage(sbuf.c_str());
      }
      break;
    }
    ptr = ptr->next;
  }  

  if(!ptr) {
   if(servercfg->debug && servercfg->debug_level >= 2 ) {
     sbuf << clear << "[" << seq_num << "]: Rule config file error no LB nodes matchs rule set";
     LogMessage(sbuf.c_str());
   }
  }

  if(ptr) {
    if(servercfg->check_node_max_clients(ptr->data, seq_num) != 0) return 0;
    if(connect_backend(s, ptr->data, "Assigned") != 0) return 0;
    return ptr->data;
  }
  return 0;
}

LBnode *LBClientRequestThread::distrib(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;
  gxList<limit_node> limit_node_list;
  gxListNode<limit_node> *limit_ptr_debug;

  int num_connections = servercfg->get_num_node_connections();
  if(num_connections == 0) num_connections = 1;

  if(servercfg->debug) {
    sbuf << clear << "Number of active server connections " << num_connections;
    LogDebugMessage(sbuf.c_str());
    sbuf << clear << "Number of refactored connections " << servercfg->REFACTORED_CONNECTIONS();
    LogDebugMessage(sbuf.c_str());
    if(servercfg->debug_level >= 2) {
      sbuf << clear << "Number of prev refactored connections " << servercfg->PREV_REFACTORED_CONNECTIONS();
      LogDebugMessage(sbuf.c_str());
    }
  }

  if(num_connections >= servercfg->REFACTORED_CONNECTIONS()) {
    servercfg->PREV_REFACTORED_CONNECTIONS(servercfg->REFACTORED_CONNECTIONS());
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_distribution_limits(limit_node_list, num_connections));
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level increased to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }
  if(num_connections < servercfg->PREV_REFACTORED_CONNECTIONS()) {
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_distribution_limits(limit_node_list, num_connections));
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level decresed to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }

  gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
  if(!limit_ptr) { // Build our default list
    servercfg->refactor_distribution_limits(limit_node_list, servercfg->PREV_REFACTORED_CONNECTIONS());
    limit_ptr = limit_node_list.GetHead();
  }

  while(limit_ptr) {
    if(limit_ptr->data.active_ptr->NUM_CONNECTIONS() < limit_ptr->data.limit) {
      if(servercfg->check_node_max_clients(limit_ptr->data.active_ptr, seq_num) != 0) {
	limit_ptr = 0;
	break;
      }
      break;
    }
    limit_ptr = limit_ptr->next;
  }

 if(!limit_ptr) return round_robin(s, servercfg->distributed_rr_node_list);

  if(connect_backend(s, limit_ptr->data.active_ptr, "Distributed") != 0) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to distributed round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->distributed_rr_node_list);
  }
  return limit_ptr->data.active_ptr;
}

LBnode *LBClientRequestThread::weighted(ClientSocket_t *s)
{
  gxString sbuf;
  gxString message;
  unsigned seq_num = s->seq_num;


  gxList<limit_node> limit_node_list;
  gxListNode<limit_node> *limit_ptr_debug;

  int num_connections = servercfg->get_num_node_connections();
  if(num_connections == 0) num_connections = 1;

  if(servercfg->debug) {
    sbuf << clear << "Number of active server connections " << num_connections;
    LogDebugMessage(sbuf.c_str());
    sbuf << clear << "Number of refactored connections " << servercfg->REFACTORED_CONNECTIONS();
    LogDebugMessage(sbuf.c_str());
    if(servercfg->debug_level >= 2) {
      sbuf << clear << "Number of prev refactored connections " << servercfg->PREV_REFACTORED_CONNECTIONS();
      LogDebugMessage(sbuf.c_str());
    }
  }

  if(num_connections >= servercfg->REFACTORED_CONNECTIONS()) {
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE(1, 0, 0)));
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level increased to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }
  if(num_connections < servercfg->PREV_REFACTORED_CONNECTIONS()) {
    servercfg->REFACTORED_CONNECTIONS(0, 0, servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE(0, 1, 0)));
    servercfg->PREV_REFACTORED_CONNECTIONS(0, 0, servercfg->REFACTORED_CONNECTIONS());
    if(servercfg->debug) {
      limit_ptr_debug = limit_node_list.GetHead();
      while(limit_ptr_debug) {
	sbuf << clear << limit_ptr_debug->data.active_ptr->node_name.c_str() <<  " refactor level decresed to " 
	     << limit_ptr_debug->data.limit << " backend connections";
	LogDebugMessage(sbuf.c_str());
	limit_ptr_debug = limit_ptr_debug->next;
      }
    }
  }

  gxListNode<limit_node> *limit_ptr = limit_node_list.GetHead();
  if(!limit_ptr) { // Build our default list
    servercfg->refactor_weighted_limits(limit_node_list, servercfg->WEIGHT_SCALE());
    limit_ptr = limit_node_list.GetHead();
  }

  while(limit_ptr) {
    if(limit_ptr->data.active_ptr->NUM_CONNECTIONS() < limit_ptr->data.limit) {
      if(servercfg->check_node_max_clients(limit_ptr->data.active_ptr, seq_num) != 0) {
	limit_ptr = 0;
	break;
      }
      break;
    }
    limit_ptr = limit_ptr->next;
  }

  if(!limit_ptr) return round_robin(s, servercfg->weighted_rr_node_list);

  if(connect_backend(s, limit_ptr->data.active_ptr, "Weighted") != 0) {
    if(servercfg->debug && servercfg->debug_level >= 5) {
      sbuf << clear << "[" << seq_num << "]: Connection error will default to weighted round robin";
      LogDebugMessage(sbuf.c_str());
    }
    return round_robin(s, servercfg->weighted_rr_node_list);
  }
  return limit_ptr->data.active_ptr;
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
