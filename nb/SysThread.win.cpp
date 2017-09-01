//==============================================================================
//
//  SysThread.win.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#ifdef OS_WIN
#include "SysThread.h"
#include <csignal>
#include <cstdint>
#include <windows.h>
#include "Debug.h"
#include "NbSignals.h"
#include "Thread.h"

//------------------------------------------------------------------------------

namespace NodeBase
{
//  Mapping of external to internal priorities.
//
const int PriorityMap[SysThread::Priority_N] =
{
   THREAD_PRIORITY_BELOW_NORMAL,  // LowPriority
   THREAD_PRIORITY_NORMAL,        // DefaultPriority
   THREAD_PRIORITY_ABOVE_NORMAL,  // SystemFaction
   THREAD_PRIORITY_HIGHEST        // WatchdogFaction
};

//------------------------------------------------------------------------------

fn_name NodeBase_SE_Handler = "NodeBase.SE_Handler";

//  Converts a Windows structured exception to a C++ exception.  The type of EX
//  is actually EXCEPTION_POINTERS, but it is not used and is therefore omitted.
//
void SE_Handler(uint32_t errval, void* ex)
{
   //  Reenable Debug functions before tracing this function.
   //
   Debug::Reset();
   Debug::ft(NodeBase_SE_Handler);

   signal_t sig = 0;

   switch(errval)                         // errval:
   {
   case DBG_CONTROL_C:                    // 0x40010005
      sig = SIGINT;
      break;

   case DBG_CONTROL_BREAK:                // 0x40010008
      sig = SIGBREAK;
      break;

   case STATUS_DATATYPE_MISALIGNMENT:     // 0x80000002
   case STATUS_ACCESS_VIOLATION:          // 0xC0000005
   case STATUS_IN_PAGE_ERROR:             // 0xC0000006
   case STATUS_INVALID_HANDLE:            // 0xC0000008
   case STATUS_NO_MEMORY:                 // 0xC0000017
      sig = SIGSEGV;
      break;

   case STATUS_ILLEGAL_INSTRUCTION:       // 0xC000001D
      sig = SIGILL;
      break;

   case STATUS_NONCONTINUABLE_EXCEPTION:  // 0xC0000025
      sig = SIGTERM;
      break;

   case STATUS_INVALID_DISPOSITION:       // 0xC0000026
   case STATUS_ARRAY_BOUNDS_EXCEEDED:     // 0xC000008C
      sig = SIGSEGV;
      break;

   case STATUS_FLOAT_DENORMAL_OPERAND:    // 0xC000008D
   case STATUS_FLOAT_DIVIDE_BY_ZERO:      // 0xC000008E
   case STATUS_FLOAT_INEXACT_RESULT:      // 0xC000008F
   case STATUS_FLOAT_INVALID_OPERATION:   // 0xC0000090
   case STATUS_FLOAT_OVERFLOW:            // 0xC0000091
   case STATUS_FLOAT_STACK_CHECK:         // 0xC0000092
   case STATUS_FLOAT_UNDERFLOW:           // 0xC0000093
   case STATUS_INTEGER_DIVIDE_BY_ZERO:    // 0xC0000094
   case STATUS_INTEGER_OVERFLOW:          // 0xC0000095
      sig = SIGFPE;
      break;

   case STATUS_PRIVILEGED_INSTRUCTION:    // 0xC0000096
      sig = SIGILL;
      break;

   case STATUS_STACK_OVERFLOW:            // 0xC00000FD
      //
      //  A stack overflow in Windows now raises the exception
      //  System.StackOverflowException, which cannot be caught.
      //  Stack checking in Thread should therefore be enabled.
      //
      sig = SIGSTACK1;
      break;

   default:
      sig = SIGTERM;
   }

   //  Handle SIG.  This usually throws an exception; in any case, it will
   //  not return here.  If it does return, there is no specific provision
   //  for reraising a structured exception, so simply return and assume
   //  that Windows will handle it, probably brutally.
   //
   Thread::HandleSignal(sig, errval);
}

//------------------------------------------------------------------------------

fn_name SysThread_Create = "SysThread.Create";

SysThread_t SysThread::Create(const ThreadEntry entry,
   const Thread* client, size_t stackSize, SysThreadId& nid)
{
   Debug::ft(SysThread_Create);

   return CreateThread(
      nullptr,                         // default security attributes
      stackSize,                       // stack size
      (LPTHREAD_START_ROUTINE) entry,  // thread entry function
      (LPVOID) client,                 // argument to entry function
      0,                               // default creation flags
      (DWORD*) &nid);                  // updates thread's identifier
}

//------------------------------------------------------------------------------

fn_name SysThread_CreateSentry = "SysThread.CreateSentry";

SysSentry_t SysThread::CreateSentry()
{
   Debug::ft(SysThread_CreateSentry);

   //  On another platform, this is likely to be a combination of a
   //  condition variable and mutex, wrapped within an object that is
   //  private to this file.
   //
   return CreateEvent(
      nullptr,   // default security attributes
      false,     // automatically reset when signaled
      false,     // initial state not signaled
      nullptr);  // unnamed
}

//------------------------------------------------------------------------------

fn_name SysThread_Delay = "SysThread.Delay";

DelayRc SysThread::Delay(msecs_t msecs)
{
   Debug::ft(SysThread_Delay);

   //  This operation can only be applied to the running thread.
   //
   if(RunningThreadId() != nid_)
   {
      Debug::SwErr(SysThread_Delay, 0, 0);
      return DelayError;
   }

   //  Wait on our timer, possibly forever.
   //
   if(msecs < 0) msecs = INFINITE;

   auto rc = WaitForSingleObject(sentry_, msecs);

   switch(rc)
   {
   case WAIT_TIMEOUT:
      //
      //  Our alarm clock went off.
      //
      return DelayCompleted;
   case WAIT_OBJECT_0:
      //
      //  Someone woke us up early, presumably because work is waiting for us.
      //
      return DelayInterrupted;
   case WAIT_ABANDONED:
      //
      //  We're the only thread that waits on sentry_, so this shouldn't occur.
      //
      Debug::SwErr(SysThread_Delay, 0, rc);
      return DelayInterrupted;
   default:
      Debug::SwErr(SysThread_Delay, GetLastError(), rc);
   }

   return DelayError;
}

//------------------------------------------------------------------------------

fn_name SysThread_Delete = "SysThread.Delete";

void SysThread::Delete(SysThread_t& thread)
{
   Debug::ft(SysThread_Delete);

   if(thread != nullptr)
   {
      CloseHandle(thread);
      thread = nullptr;
   }
}

//------------------------------------------------------------------------------

fn_name SysThread_DeleteSentry = "SysThread.DeleteSentry";

void SysThread::DeleteSentry(SysSentry_t& sentry)
{
   Debug::ft(SysThread_DeleteSentry);

   if(sentry != nullptr)
   {
      CloseHandle(sentry);
      sentry = nullptr;
   }
}

//------------------------------------------------------------------------------

fn_name SysThread_Interrupt = "SysThread.Interrupt";

bool SysThread::Interrupt()
{
   Debug::ft(SysThread_Interrupt);

   //  Signal the thread's timer to wake it up.  If the thread is not
   //  delaying, this has the side effect of immediately reawakening
   //  the thread the next time it tries to sleep.
   //
   if(SetEvent(sentry_) != 0) return true;
   Debug::SwErr(SysThread_Interrupt, GetLastError(), 1);
   return false;
}

//------------------------------------------------------------------------------

void SysThread::Patch(sel_t selector, void* arguments)
{
   Permanent::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

void SysThread::RegisterForSignal(signal_t sig, sighandler_t handler)
{
   signal(sig, handler);

   //  If the platform supports sigaction, it is preferred.  It should mask
   //  signals that do not point to an error in the signal handler itself.
   //  This is only a sketch.  For example, SIGSEGV should use sigaltstack
   //  to safely catch a stack overrun.
   //
   //  sigaction action;
   //  sigset_t  block_mask;
   //
   //  sigemptyset(&block_mask);
   //  sigaddset(&block_mask, SIGTERM);
   //  sigaddset(&block_mask, SIGINT);
   //  sigaddset(&block_mask, SIGQUIT);
   //  sigaddset(&block_mask, SIGVTALRM);
   //
   //  action.sa_handler = handler;
   //  action.sa_mask = block_mask;
   //  action.sa_flags = 0;
   //
   //  sigaction(sig, &action, nullptr);
}

//------------------------------------------------------------------------------

SysThreadId SysThread::RunningThreadId()
{
   return GetCurrentThreadId();
}

//------------------------------------------------------------------------------

fn_name SysThread_SetPriority = "SysThread.SetPriority";

bool SysThread::SetPriority(Priority prio)
{
   Debug::ft(SysThread_SetPriority);

   if(!SetThreadPriority(nthread_, PriorityMap[prio]))
   {
      status_.set(SetPriorityFailed);
      return false;
   }

   status_.reset(SetPriorityFailed);
   return true;
}

//------------------------------------------------------------------------------

fn_name SysThread_Start = "SysThread.Start";

signal_t SysThread::Start()
{
   Debug::ft(SysThread_Start);

   //  This is also invoked when recovering from a trap, so see if a stack
   //  overflow occurred.  Some of these are irrecoverable, in which case
   //  returning SIGSTACK2 causes the thread to exit and be recreated.
   //
   if(status_.test(StackOverflowed))
   {
      if(_resetstkoflw() == 0)
      {
         Debug::SwErr(SysThread_Start, status_.to_string(), nid_);
         return SIGSTACK2;
      }

      status_.reset(StackOverflowed);
   }

   //  The translator for Windows structured exceptions must be installed
   //  on a per-thread basis.
   //
   _set_se_translator((_se_translator_function) SE_Handler);
   return 0;
}

//------------------------------------------------------------------------------

fn_name SysThread_Wrap = "SysThread.Wrap";

SysThread_t SysThread::Wrap()
{
   Debug::ft(SysThread_Wrap);

   //  Set our overall process priority and return a handle to our thread.
   //
   auto process = GetCurrentProcess();
   SetPriorityClass(process, HIGH_PRIORITY_CLASS);

   SysThread_t clone = GetCurrentThread();
   SysThread_t nthread;

   if(!DuplicateHandle(process, clone, process,
      &nthread, 0, false, DUPLICATE_SAME_ACCESS))
   {
      Debug::Assert(false);
   }

   return nthread;
}
}
#endif