// ------------------------------- //
// -------- Start of File -------- //
// ------------------------------- //
// ----------------------------------------------------------- // 
// C++ Source Code File
// C++ Compiler Used: GNU, Intel
// Produced By: DataReel Software Development Team
// File Creation Date: 06/17/2016
// Date Last Modified: 09/17/2016
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

Process and signal functions for Datareel load balancer.

*/
// ----------------------------------------------------------- // 
#include "gxdlcode.h"

#if defined (__USE_ANSI_CPP__) // Use the ANSI Standard C++ library
#include <iostream>
using namespace std; // Use unqualified names for Standard C++ library
#else // Use the old iostream library by default
#include <iostream.h>
#endif // __USE_ANSI_CPP__

#include "drlb_server.h"
#include "m_proc.h"
#include "m_log.h"

#include <pthread.h>
#include <signal.h>

void *ProcessThread::ThreadEntryRoutine(gxThread_t *thread)
{
  while(servercfg->process_loop) { 
    if(servercfg->kill_server) break;
    if(servercfg->restart_server) break;

    servercfg->process_is_locked = 1;
    if(servercfg->process_lock.MutexLock() != 0) {
      LogProcMessage("ERROR - Process thread could not obtain a mutex lock");
      servercfg->process_is_locked = 0;
      break;
    }
    // Block this thread from its own execution 
    servercfg->process_cond.ConditionWait(&servercfg->process_lock);
    servercfg->process_lock.MutexUnlock();
    servercfg->process_is_locked = 0;

    if(servercfg->accept_clients == 0) {
      // Server thread has stopped, end the process thread here
      break;
    }
  }
  sSleep(1); // Allow for I/O recovery time before exiting the program
  return (void *)0;
}

void signal_handler_IO(int status)
// Signal handler. Sets wait_flag to 0, to indicate the above loop that  
// characters have been received. 
{
  // Add all signal I/O calls and variables here
}

int StopThreads()
{
  if(servercfg->debug) LogProcMessage("Entering stop threads() function");

  unsigned num_threads = 0;
  gxString message;
  LBServerThread t;
  int error_level = 0;

  if(servercfg->debug) LogProcMessage("Checking client request pool status");
  if(servercfg->client_request_pool) {
    if(!servercfg->client_request_pool->IsEmpty()) {
      // 10/25/2026: Client threads are detached and cannot be canceled.
      // The old code tried to cancel them and then freed their thread
      // records while they were still running, which crashed the process
      // on shutdown. With accept_clients cleared every relay loop exits
      // within about one second, so wait for them to finish instead.
      int wait_secs = servercfg->write_timeout_secs + 5;
      while(1) {
	num_threads = 0;
	thrPoolNode *ptr = servercfg->client_request_pool->GetHead();
	while(ptr) {
	  if(ptr->GetThreadPtr()->GetThreadState() == gxTHREAD_STATE_RUNNING) num_threads++;
	  ptr = ptr->GetNext();
	}
	if(num_threads == 0 || wait_secs-- <= 0) break;
	if(servercfg->debug) {
	  message << clear << "Waiting for " << num_threads << " client threads to finish";
	  LogProcMessage(message.c_str());
	}
	sSleep(1);
      }
      if(num_threads == 0) {
	LogProcMessage("All client threads have finished");
	t.DestroyThreadPool(servercfg->client_request_pool, 0);
	servercfg->client_request_pool = 0;
      }
      else {
	message << clear << num_threads << " client threads are still running and will end with the process";
	LogProcMessage(message.c_str());
      }
    }
    else {
      LogProcMessage("No open connections");
    }
  }


  if(servercfg->debug) LogProcMessage("Checking LB server thread status");
  if(servercfg->server_thread) { 
    if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping LB server thread");
      t.CancelThread(servercfg->server_thread);
      sSleep(5); // Allow I/O recovery time
      t.JoinThread(servercfg->server_thread);
      if(servercfg->server_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("LB server thread was stopped");
	delete servercfg->server_thread;
	servercfg->server_thread = 0;
      }
      else {
	LogProcMessage("LB server thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Server thread not running");
    }
  }

  if(error_level != 0) return error_level;

  if(servercfg->debug) LogProcMessage("Checking stats thread status");
  if(servercfg->stats_thread) { 
    StatsThread stats;
    if(servercfg->stats_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping stats thread");
      stats.CancelThread(servercfg->stats_thread);
      sSleep(1);
      stats.JoinThread(servercfg->stats_thread);
      if(servercfg->stats_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("Stats thread was stopped");
	delete servercfg->stats_thread;
	servercfg->stats_thread = 0;
      }
      else {
	LogProcMessage("LB stats thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Stats thread not running");
    }
  }

  if(error_level != 0) return error_level;
  
  if(servercfg->debug) LogProcMessage("Checking log thread status");
  if(servercfg->log_thread) { 
    LogThread log;
    if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_RUNNING) {
      LogProcMessage("Stopping log thread");
      log.CancelThread(servercfg->log_thread);
      sSleep(1);
      log.JoinThread(servercfg->log_thread);
      if(servercfg->log_thread->GetThreadState() == gxTHREAD_STATE_CANCELED) {
	LogProcMessage("Log thread was stopped");
	delete servercfg->log_thread;
	servercfg->log_thread = 0;
      }
      else {
	LogProcMessage("LB log thread did not shutdown properly");
	error_level++;
      }
    }
    else {
      LogProcMessage("LB Log thread not running");
    }
  }
  
  if(servercfg->debug) LogProcMessage("Stop threads() call complete");
  return error_level;
}

int StopProc() 
{
  if(servercfg->debug && servercfg->log_level == 0) servercfg->log_level = 1;
  servercfg->accept_clients = 0;
  servercfg->kill_server = 1;
  servercfg->process_loop = 0;
  servercfg->process_cond.ConditionSignal(); 

  int retries = 3;
  while(StopThreads() != 0 && retries--) sleep(1);
 
  LogThread log_t;
  log_t.flush_all_logs();
  if(servercfg->logfile) servercfg->logfile.Close();
  if(servercfg->stats_file) servercfg->stats_file.Close();

  NT_print("Sending termination signal to end process");
  sSleep(1);

  return 1;
}

void ExitProc(int return_code)
{
  NT_print("Exiting DRLB process...");
  exit(return_code);
}

// 10/25/2026: Signals are no longer handled in a signal handler. The
// old handler locked mutexes, allocated memory and stopped threads from
// signal context, which can deadlock. All threads now block these
// signals and one thread waits for them with sigwait() and calls
// termination_handler() as a normal function.
static void drlb_signal_set(sigset_t *set)
{
  sigemptyset(set);
  sigaddset(set, SIGINT);
  sigaddset(set, SIGQUIT);
  sigaddset(set, SIGTERM);
  sigaddset(set, SIGHUP);
  sigaddset(set, SIGTSTP);
  sigaddset(set, SIGUSR1);
  sigaddset(set, SIGUSR2);
}

static void *signal_thread(void *)
{
  sigset_t set;
  drlb_signal_set(&set);
  while(1) {
    int signum = 0;
    if(sigwait(&set, &signum) != 0) continue;
    termination_handler(signum);
  }
  return (void *)0;
}

int StartSignalThread()
// Call before any other thread is created so every thread inherits the
// blocked signal mask. Returns 0 if successful.
{
  sigset_t set;
  drlb_signal_set(&set);
  if(pthread_sigmask(SIG_BLOCK, &set, 0) != 0) return 1;
  pthread_t tid;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  int rv = pthread_create(&tid, &attr, signal_thread, 0);
  pthread_attr_destroy(&attr);
  return rv;
}

void termination_handler(int signum)
// Handles process termination and debug level signals. Called from
// the signal thread, not from a signal handler.
{
  gxString sbuf;
  int retries = 3;

  if(signum == SIGINT) {
    LogProcMessage("Process interrupted with Ctrl-C");
    while(!StopProc() && --retries) sSleep(1);
    if(!retries) ExitProc(1);
    ExitProc(0);
  }
  if(signum == SIGQUIT) {
    LogProcMessage("Process interrupted Ctrl-backslash");
    return;
  }
  if(signum == SIGTERM) {
    LogProcMessage("Process terminated by kill command");
    while(!StopProc() && --retries) sSleep(1);
    if(!retries) ExitProc(1);
    ExitProc(0);
  }
  if(signum == SIGHUP) {
    LogProcMessage("Process received hangup");
    return;
  }
  if(signum == SIGTSTP) {
    LogProcMessage("Process received suspend from Ctrl-Z");
    return;
  }
  if(signum == SIGABRT) {
    LogProcMessage("Process received SIGABRT");
    return;
  }
  if(signum == SIGUSR1) {
    LogProcMessage("Process received SIGUSR1, lowering debug level");
    
    if(!servercfg->debug) LogProcMessage("Debug mode is disabled");

    if(servercfg->debug && servercfg->debug_level > 0) {
      servercfg->debug_level--;
      if(servercfg->debug_level <= 0) {
	LogProcMessage("Debug mode has been disabled");
	servercfg->debug = 0;
	servercfg->debug_level = 0;
      }
      else {
	sbuf << clear << "Debug level decresed to " << servercfg->debug_level;
	LogProcMessage(sbuf.c_str());
      }
    }
    return;
  }
  if(signum == SIGUSR2) {
    LogProcMessage("Process received SIGUSR2, raising debug level");

    if(servercfg->debug) {
      LogProcMessage("Debug mode is enabled");
    }

    if(!servercfg->debug) {
      LogProcMessage("Debug mode has been enabled");
      servercfg->debug = 1;
      servercfg->debug_level = 1;
    }

    if(servercfg->debug && servercfg->debug_level >= 1) {
      servercfg->debug_level++;
      if(servercfg->debug_level >= 5) {
	LogProcMessage("Debug mode set to level 5");
	servercfg->debug = 1;
	servercfg->debug_level = 5;
      }
      else {
	sbuf << clear << "Debug level increased to " << servercfg->debug_level;
	LogProcMessage(sbuf.c_str());
      }
    }
    return;
  }
}

// Pauses for a specified number of seconds. 
void sSleep(unsigned long seconds)
{
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, seconds);
    break;
  }
}

// Pauses for a specified number of nanoseconds. 
void nSleep(unsigned long nanoseconds)
{
  gxMutex t_mutex;
  gxCondition t_condition;
  t_mutex.MutexLock();
  while(1) {
    t_condition.ConditionTimedWait(&t_mutex, 0, nanoseconds);
    break;
  }
}

// Pauses for a specified number of microseconds. 
void uSleep(unsigned long microseconds)
{
  // Convert microseconds to nanoseconds
  unsigned long nseconds = microseconds * 1000; 
  nSleep(nseconds);
}

// Pauses for a specified number of milliseconds. 
void mSleep(unsigned long  milliseconds)
{
  // Convert millisecond to nanoseconds
  unsigned long nseconds = milliseconds * (1000 * 1000); 
  nSleep(nseconds);
}
// ----------------------------------------------------------- // 
// ------------------------------- //
// --------- End of File --------- //
// ------------------------------- //
