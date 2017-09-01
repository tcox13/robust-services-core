//==============================================================================
//
//  InvokerPool.h
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#ifndef INVOKERPOOL_H_INCLUDED
#define INVOKERPOOL_H_INCLUDED

#include "Dynamic.h"
#include <cstddef>
#include <iosfwd>
#include <string>
#include "Clock.h"
#include "Factory.h"
#include "Message.h"
#include "NbTypes.h"
#include "RegCell.h"
#include "Registry.h"
#include "SbTypes.h"
#include "SysTypes.h"

namespace SessionBase
{
   class InvokerPoolStats;
   class InvokerWork;
}

using namespace NodeBase;

//------------------------------------------------------------------------------

namespace SessionBase
{
//  An invoker pool consists of a set of invoker threads.  It also has a
//  set of work queues, one for each message priority.  Each subclass of
//  InvokerPool is a singleton whose invokers run in the same scheduler
//  faction.
//
class InvokerPool : public Dynamic
{
   friend class Context;
   friend class InvokerPoolRegistry;
   friend class InvokerThread;
   friend class Message;
   friend class Registry< InvokerPool >;
   friend class SbInputHandler;
public:
   //> The maximum number of invoker threads allowed in a pool.
   //
   static const size_t MaxInvokers;

   //  Returns the pool's scheduler faction.
   //
   Faction GetFaction() const { return Faction(faction_.GetId()); }

   //  Returns the length of the work queue associated with PRIO.
   //
   size_t WorkQCurrLength(Message::Priority prio) const;

   //  Returns a work queue's maximum length during the current
   //  statistics interval.
   //
   size_t WorkQMaxLength(Message::Priority prio) const;

   //  Returns a work queue's maximum delay during the current
   //  statistics interval.
   //
   msecs_t WorkQMaxDelay(Message::Priority prio) const;

   //  Displays statistics.
   //
   virtual void DisplayStats(std::ostream& stream) const;

   //  Returns the offset to faction_.
   //
   static ptrdiff_t CellDiff();

   //  Overridden for restarts.
   //
   virtual void Startup(RestartLevel level) override;

   //  Overridden to display member variables.
   //
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   virtual void Patch(sel_t selector, void* arguments) override;
protected:
   //  Defines a pool of invoker threads that will run in FACTION and adds
   //  it to the global registry of invoker pools.  parmKey is the key for
   //  the configuration parameter that controls the number of threads.
   //  Protected because subclasses should be singletons.
   //
   InvokerPool(Faction faction, const std::string& parmKey);

   //  Deletes all contexts in the work queues.  Protected because subclasses
   //  should be singletons.
   //
   virtual ~InvokerPool();
private:
   //  Returns true if ingress work should be rejected.  Each pool
   //  should override this to protect against overload.
   //
   virtual bool RejectIngressWork() const { return false; }

   //  Adds THREAD to the set of invokers.
   //
   bool BindThread(InvokerThread& thread);

   //  Removes THREAD from the set of invokers.
   //
   void UnbindThread(InvokerThread& thread);

   //  Invoked when an input handler receives BUFF.  Passes BUFF to the
   //  appropriate factory to wrap it with a message and then invokes
   //  ReceiveMsg.
   //
   bool ReceiveBuff(SbIpBufferPtr& buff, bool atIoLevel);

   //  Invoked when Message::Send notices that MSG can be moved from one
   //  context to another because it is intraprocessor.  Also invoked by
   //  ReceiveBuff, in which case atIoLevel is true.  Passes MSG to the
   //  appropriate factory to have it queued on a context, and then queues
   //  the context on the appropriate work queue.
   //
   bool ReceiveMsg(Message& msg, bool atIoLevel);

   //  Wakes up a sleeping invoker thread when ReceiveMsg queues work.
   //
   void KickThread();

   //  Scans the work queues in priority order to find a work item.
   //
   Context* FindWork();

   //  Called by an invoker to process items on the work queues.
   //
   void ProcessWork();

   //  Called when a context is removed from the work queue associated
   //  with PRIO.
   //
   void Dequeued(Message::Priority prio) const;

   //  Called when a context is added to the work queue associated
   //  with PRIO.
   //
   void Enqueued(Message::Priority prio) const;

   //  Called to record the time that a message waited on a work queue
   //  before being processed.
   //
   void RecordDelay(Message::Priority prio, msecs_t msecs) const;

   //  Returns CTX to the progress work queue after it has processed
   //  messages of immediate priority.
   //
   void Requeue(Context& ctx);

   //  Returns the number of invoker threads that are running or sleeping.
   //
   size_t ReadyCount() const;

   //  Called by an invoker that is being scheduled out.
   //
   void ScheduledOut();

   //  Returns true if a lost message log should be generated for RES.
   //
   static bool GenerateLog(Factory::Rc rc);

   //  Generates a log when ReceiveBuff fails.
   //
   bool LogLostBuff(SbIpBuffer& buff, FactoryId fid, Factory::Rc rc) const;

   //  Generates a log when ReceiveMsg fails.
   //
   bool LogLostMsg(Message& msg, Factory::Rc rc, TransTrace* tt) const;

   //  Captures the arrival of external message MSG at factory FAC.
   //
   static TransTrace* TraceRxNet(Message& msg, const Factory& fac);

   //  Overridden to mark objects in the work queues as being in use.
   //
   virtual void ClaimBlocks() override;

   //  Overridden to prohibit copying.
   //
   InvokerPool(const InvokerPool& that);
   void operator=(const InvokerPool& that);

   //  The scheduler faction in which the pool's invokers run.
   //
   RegCell faction_;

   //  The desired number of invokers in the pool.
   //
   word poolSize_;

   //  The configuration parameter for the number of invokers in the pool.
   //
   CfgIntParmPtr cfgInvokers_;

   //  The pool's pending work.
   //
   std::unique_ptr< InvokerWork > work_[Message::MaxPriority + 1];

   //  The pool's invoker(s)
   //
   Registry< InvokerThread > invokers_;

   //  Used while the audit traverses the work queues.
   //
   bool corrupt_;

   //  The pool's statistics.
   //
   std::unique_ptr< InvokerPoolStats > stats_;
};
}
#endif