//==============================================================================
//
//  ObjectPoolRegistry.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "ObjectPoolRegistry.h"
#include <memory>
#include <ostream>
#include <string>
#include "CfgBoolParm.h"
#include "CfgParmRegistry.h"
#include "Debug.h"
#include "Formatters.h"
#include "NbCliParms.h"
#include "ObjectPool.h"
#include "ObjectPoolAudit.h"
#include "Singleton.h"
#include "StatisticsGroup.h"
#include "SysTypes.h"
#include "ThisThread.h"
#include "Tool.h"
#include "ToolTypes.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace NodeBase
{
fixed_string ObjPoolTraceToolName = "ObjPoolTracer";
fixed_string ObjPoolTraceToolExpl = "traces pooled objects";

class ObjPoolTraceTool : public Tool
{
   friend class Singleton< ObjPoolTraceTool >;
private:
   ObjPoolTraceTool() : Tool(ObjPoolTracer, 'o', true) { }
   virtual const char* Name() const override { return ObjPoolTraceToolName; }
   virtual const char* Expl() const override { return ObjPoolTraceToolExpl; }
};

//------------------------------------------------------------------------------

class ObjectPoolStatsGroup : public StatisticsGroup
{
public:
   ObjectPoolStatsGroup();
   ~ObjectPoolStatsGroup();
   virtual void DisplayStats(ostream& stream, id_t id) const override;
};

//------------------------------------------------------------------------------

fn_name ObjectPoolStatsGroup_ctor = "ObjectPoolStatsGroup.ctor";

ObjectPoolStatsGroup::ObjectPoolStatsGroup() :
   StatisticsGroup("Object Pools [ObjectPoolId]")
{
   Debug::ft(ObjectPoolStatsGroup_ctor);
}

//------------------------------------------------------------------------------

fn_name ObjectPoolStatsGroup_dtor = "ObjectPoolStatsGroup.dtor";

ObjectPoolStatsGroup::~ObjectPoolStatsGroup()
{
   Debug::ft(ObjectPoolStatsGroup_dtor);
}

//------------------------------------------------------------------------------

fn_name ObjectPoolStatsGroup_DisplayStats = "ObjectPoolStatsGroup.DisplayStats";

void ObjectPoolStatsGroup::DisplayStats(ostream& stream, id_t id) const
{
   Debug::ft(ObjectPoolStatsGroup_DisplayStats);

   StatisticsGroup::DisplayStats(stream, id);

   auto reg = Singleton< ObjectPoolRegistry >::Instance();

   if(id == 0)
   {
      auto& pools = reg->Pools();

      for(auto p = pools.First(); p != nullptr; pools.Next(p))
      {
         p->DisplayStats(stream);
      }
   }
   else
   {
      auto p = reg->Pool(id);

      if(p == nullptr)
      {
         stream << spaces(2) << NoPoolExpl << CRLF;
         return;
      }

      p->DisplayStats(stream);
   }
}

//==============================================================================

bool ObjectPoolRegistry::NullifyObjectData_ = false;

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_ctor = "ObjectPoolRegistry.ctor";

ObjectPoolRegistry::ObjectPoolRegistry()
{
   Debug::ft(ObjectPoolRegistry_ctor);

   Singleton< ObjPoolTraceTool >::Instance();
   pools_.Init(ObjectPool::MaxId + 1, ObjectPool::CellDiff(), MemProt);
   statsGroup_.reset(new ObjectPoolStatsGroup);
   nullifyObjectData_.reset(new CfgBoolParm("NullifyObjectData", "F",
      &NullifyObjectData_, "set to nullify an object's data after its vptr"));
   Singleton< CfgParmRegistry >::Instance()->BindParm(*nullifyObjectData_);
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_dtor = "ObjectPoolRegistry.dtor";

ObjectPoolRegistry::~ObjectPoolRegistry()
{
   Debug::ft(ObjectPoolRegistry_dtor);
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_AuditPools = "ObjectPoolRegistry.AuditPools";

void ObjectPoolRegistry::AuditPools() const
{
   Debug::ft(ObjectPoolRegistry_AuditPools);

   auto thread = Singleton< ObjectPoolAudit >::Instance();

   //  This code is stateful.  When it is reentered after an exception, it
   //  resumes execution at the phase and pool where the exception occurred.
   //
   while(true)
   {
      switch(thread->phase_)
      {
      case ObjectPoolAudit::CheckingFreeq:
         //
         //  Audit the pool's free queue.
         //
         while(thread->pid_ <= ObjectPool::MaxId)
         {
            auto pool = pools_.At(thread->pid_);

            if(pool != nullptr)
            {
               pool->AuditFreeq();
               ThisThread::Pause();
            }

            ++thread->pid_;
         }

         thread->phase_ = ObjectPoolAudit::ClaimingBlocks;
         thread->pid_ = NIL_ID;
         break;

      case ObjectPoolAudit::ClaimingBlocks:
         //
         //  Claim in-use blocks in each pool.  Each ClaimBlocks function
         //  finds its blocks in an application-specific way.  The blocks
         //  must be claimed after *all* blocks, in *all* pools, have been
         //  marked, because some ClaimBlocks functions claim blocks from
         //  multiple pools.
         //
         while(thread->pid_ <= ObjectPool::MaxId)
         {
            auto pool = pools_.At(thread->pid_);

            if(pool != nullptr)
            {
               pool->ClaimBlocks();
               ThisThread::Pause();
            }

            ++thread->pid_;
         }

         thread->phase_ = ObjectPoolAudit::RecoveringBlocks;
         thread->pid_ = NIL_ID;
         break;

      case ObjectPoolAudit::RecoveringBlocks:
         //
         //  For each object pool, recover any block that is still marked.
         //  Such  a block is an orphan that is neither on the free queue
         //  nor in use by an application.
         //
         while(thread->pid_ <= ObjectPool::MaxId)
         {
            auto pool = pools_.At(thread->pid_);

            if(pool != nullptr)
            {
               pool->RecoverBlocks();
               ThisThread::Pause();
            }

            ++thread->pid_;
         }

         thread->phase_ = ObjectPoolAudit::CheckingFreeq;
         thread->pid_ = NIL_ID;
         return;

      default:
         //
         //  An unknown phase.
         //
         Debug::SwErr
            (ObjectPoolRegistry_AuditPools, thread->phase_, thread->pid_);
         thread->phase_ = ObjectPoolAudit::CheckingFreeq;
         thread->pid_ = NIL_ID;
         return;
      }
   }
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_BindPool = "ObjectPoolRegistry.BindPool";

bool ObjectPoolRegistry::BindPool(ObjectPool& pool)
{
   Debug::ft(ObjectPoolRegistry_BindPool);

   return pools_.Insert(pool);
}

//------------------------------------------------------------------------------

void ObjectPoolRegistry::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Protected::Display(stream, prefix, options);

   stream << prefix << "statsGroup        : ";
   stream << strObj(statsGroup_.get()) << CRLF;
   stream << prefix << "NullifyObjectData : ";
   stream << NullifyObjectData_ << CRLF;
   stream << prefix << "nullifyObjectData : ";
   stream << strObj(nullifyObjectData_.get()) << CRLF;

   stream << prefix << "pools [ObjectPoolId]" << CRLF;
   pools_.Display(stream, prefix + spaces(2), options);
}

//------------------------------------------------------------------------------

void ObjectPoolRegistry::Patch(sel_t selector, void* arguments)
{
   Protected::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

ObjectPool* ObjectPoolRegistry::Pool(ObjectPoolId pid) const
{
   return pools_.At(pid);
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_Shutdown = "ObjectPoolRegistry.Shutdown";

void ObjectPoolRegistry::Shutdown(RestartLevel level)
{
   Debug::ft(ObjectPoolRegistry_Shutdown);

   for(auto p = pools_.Last(); p != nullptr; pools_.Prev(p))
   {
      p->Shutdown(level);
   }

   if(level < RestartCold) return;

   statsGroup_.release();
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_Startup = "ObjectPoolRegistry.Startup";

void ObjectPoolRegistry::Startup(RestartLevel level)
{
   Debug::ft(ObjectPoolRegistry_Startup);

   if(statsGroup_ == nullptr) statsGroup_.reset(new ObjectPoolStatsGroup);

   for(auto p = pools_.First(); p != nullptr; pools_.Next(p))
   {
      p->Startup(level);
   }
}

//------------------------------------------------------------------------------

fn_name ObjectPoolRegistry_UnbindPool = "ObjectPoolRegistry.UnbindPool";

void ObjectPoolRegistry::UnbindPool(ObjectPool& pool)
{
   Debug::ft(ObjectPoolRegistry_UnbindPool);

   pools_.Erase(pool);
}
}