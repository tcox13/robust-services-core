//==============================================================================
//
//  Service.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "Service.h"
#include <ostream>
#include <string>
#include "Algorithms.h"
#include "Debug.h"
#include "Formatters.h"
#include "SbHandlers.h"
#include "ServiceRegistry.h"
#include "Singleton.h"
#include "Trigger.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace SessionBase
{
fixed_string AnalyzeMsgEventStr      = "AnalyzeMsgEvent";
fixed_string AnalyzeSapEventStr      = "AnalyzeSapEvent";
fixed_string AnalyzeSnpEventStr      = "AnalyzeSnpEvent";
fixed_string InitiationEventStr      = "InitiationEvent";
fixed_string ForceTransitionEventStr = "ForceTransitionEvent";
fixed_string MediaFailureEventStr    = "MediaFailureEvent";

//------------------------------------------------------------------------------

fn_name Service_ctor = "Service.ctor";

Service::Service(Id sid, bool modifiable, bool modifier) :
   status_(NotRegistered),
   modifiable_(modifiable),
   modifier_(modifier)
{
   Debug::ft(Service_ctor);

   sid_.SetId(sid);

   states_.Init(State::MaxId + 1, State::CellDiff(), MemProt);
   handlers_.Init(EventHandler::MaxId + 1, 0, MemProt, false);
   for(auto i = 0; i <= Event::MaxId; ++i) eventNames_[i] = nullptr;
   triggers_.Init(Trigger::MaxId + 1, 0, MemProt, false);

   //  Add the service to the global service registry.
   //
   if(!Singleton< ServiceRegistry >::Instance()->BindService(*this)) return;

   status_ = Enabled;

   //  Register system event handlers and their corresponding event names.
   //  All services require the Analyze Message event handler.  There is
   //  no system-defined Media Failure event handler, but all services can
   //  receive this event.
   //
   BindSystemHandler
      (*Singleton< SbAnalyzeMessage >::Instance(), EventHandler::AnalyzeMsg);
   BindEventName(AnalyzeMsgEventStr, Event::AnalyzeMsg);
   BindEventName(AnalyzeSapEventStr, Event::AnalyzeSap);
   BindEventName(AnalyzeSnpEventStr, Event::AnalyzeSnp);
   BindEventName(MediaFailureEventStr, Event::MediaFailure);

   //  Only modifiers require the Analyze SAP and Analyze SNP event handlers.
   //
   if(modifier_)
   {
      BindSystemHandler
         (*Singleton< SbAnalyzeSap >::Instance(), EventHandler::AnalyzeSap);
      BindSystemHandler
         (*Singleton< SbAnalyzeSnp >::Instance(), EventHandler::AnalyzeSnp);
   }

   //  Only modifiable services require the Force Transition event handler.
   //
   if(modifiable_)
   {
      BindSystemHandler(*Singleton< SbForceTransition >::Instance(),
         EventHandler::ForceTransition);
      BindEventName(ForceTransitionEventStr, Event::ForceTransition);
   }

   //  Modifiable and modifier services require the initiation event handler.
   //
   if(modifiable_ || modifier_)
   {
      BindSystemHandler(*Singleton< SbInitiationReq >::Instance(),
         EventHandler::InitiationReq);
      BindEventName(InitiationEventStr, Event::InitiationReq);
   }
}

//------------------------------------------------------------------------------

fn_name Service_dtor = "Service.dtor";

Service::~Service()
{
   Debug::ft(Service_dtor);

   Singleton< ServiceRegistry >::Instance()->UnbindService(*this);
}

//------------------------------------------------------------------------------

fn_name Service_AllocModifier = "Service.AllocModifier";

ServiceSM* Service::AllocModifier() const
{
   Debug::ft(Service_AllocModifier);

   //  It is either illegal to allocate this service as a modifier, or it
   //  should have overridden this function.
   //
   Debug::SwErr(Service_AllocModifier, Sid(), modifier_);
   return nullptr;
}

//------------------------------------------------------------------------------

fn_name Service_BindEventName = "Service.BindEventName";

bool Service::BindEventName(const char* name, EventId eid)
{
   Debug::ft(Service_BindEventName);

   //  Before registering the event name, check that
   //  o the service is already registered
   //  o the event name actually exists
   //  o the event identifier is valid
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_BindEventName, Sid(), 0);
      return false;
   }

   if(!Event::IsValidId(eid))
   {
      Debug::SwErr(Service_BindEventName, eid, 1);
      return false;
   }

   //  If an event name is already registered against EID,
   //  overwrite it after generating a warning.
   //
   if(eventNames_[eid] != nullptr)
   {
      Debug::SwErr(Service_BindEventName, eid, 2);
   }

   eventNames_[eid] = name;
   return true;
}

//------------------------------------------------------------------------------

fn_name Service_BindHandler = "Service.BindHandler";

bool Service::BindHandler(EventHandler& handler, EventHandlerId ehid)
{
   Debug::ft(Service_BindHandler);

   //  Before registering the event handler, check that
   //  o the service is already registered
   //  o the event handler identifier is valid
   //  o an event handler is not already registered against that identifier
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_BindHandler, pack2(Sid(), ehid), 0);
      return false;
   }

   if(!EventHandler::AppCanRegister(ehid))
   {
      Debug::SwErr(Service_BindHandler, pack2(Sid(), ehid), 1);
      return false;
   }

   return handlers_.Insert(handler, ehid);
}

//------------------------------------------------------------------------------

fn_name Service_BindState = "Service.BindState";

bool Service::BindState(State& state)
{
   Debug::ft(Service_BindState);

   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_BindState, Sid(), 0);
      return false;
   }

   return states_.Insert(state);
}

//------------------------------------------------------------------------------

fn_name Service_BindSystemHandler = "Service.BindSystemHandler";

bool Service::BindSystemHandler(EventHandler& handler, EventHandlerId ehid)
{
   Debug::ft(Service_BindSystemHandler);

   //  Before registering the event handler, check that
   //  o the service is already registered
   //  o the event handler identifier is valid
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_BindSystemHandler, Sid(), 0);
      return false;
   }

   if(ehid >= EventHandler::NextId)
   {
      Debug::SwErr(Service_BindSystemHandler, ehid, 1);
      return false;
   }

   return handlers_.Insert(handler, ehid);
}

//------------------------------------------------------------------------------

fn_name Service_BindTrigger = "Service.BindTrigger";

bool Service::BindTrigger(Trigger& trigger)
{
   Debug::ft(Service_BindTrigger);

   auto tid = trigger.Tid();

   //  Before registering the trigger, check that
   //  o the service is already registered
   //  o the service allows modifiers
   //  o the trigger's identifier is valid
   //  o a trigger is not already registered against that identifier
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_BindTrigger, Sid(), 0);
      return false;
   }

   if(!modifiable_)
   {
      Debug::SwErr(Service_BindTrigger, Sid(), 1);
      return false;
   }

   return triggers_.Insert(trigger, tid);
}

//------------------------------------------------------------------------------

ptrdiff_t Service::CellDiff()
{
   int local;
   auto fake = reinterpret_cast< const Service* >(&local);
   return ptrdiff(&fake->sid_, fake);
}

//------------------------------------------------------------------------------

fn_name Service_Disable = "Service.Disable";

bool Service::Disable()
{
   Debug::ft(Service_Disable);

   //  If the service is registered, disable it.
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_Disable, Sid(), 0);
      return false;
   }

   status_ = Disabled;
   return true;
}

//------------------------------------------------------------------------------

void Service::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Protected::Display(stream, prefix, options);

   stream << prefix << "sid        : " << sid_.to_str() << CRLF;
   stream << prefix << "status     : " << status_ << CRLF;
   stream << prefix << "modifiable : ";
   stream << modifiable_ << CRLF;
   stream << prefix << "modifier   : ";
   stream << modifier_ << CRLF;
   stream << prefix << "states [State::Id]" << CRLF;
   states_.Display(stream, prefix + spaces(2), options);
   stream << prefix << "handlers [EventHandlerId]" << CRLF;
   handlers_.Display(stream, prefix + spaces(2), options);

   auto lead = prefix + spaces(2);
   stream << prefix << "eventNames [EventId]" << CRLF;

   for(auto i = 0; i <= Event::MaxId; ++i)
   {
      if(eventNames_[i] != nullptr)
      {
         stream << lead << strIndex(i) << eventNames_[i] << CRLF;
      }
   }

   stream << prefix << "triggers [TriggerId]" << CRLF;
   triggers_.Display(stream, lead, options);
}

//------------------------------------------------------------------------------

fn_name Service_Enable = "Service.Enable";

bool Service::Enable()
{
   Debug::ft(Service_Enable);

   //  If the service is registered, enable it.
   //
   if(status_ == NotRegistered)
   {
      Debug::SwErr(Service_Enable, Sid(), 0);
      return false;
   }

   status_ = Enabled;
   return true;
}

//------------------------------------------------------------------------------

const char* Service::EventName(EventId eid) const
{
   if(!Event::IsValidId(eid)) return nullptr;

   return eventNames_[eid];
}

//------------------------------------------------------------------------------

Trigger* Service::GetTrigger(TriggerId tid) const
{
   return triggers_.At(tid);
}

//------------------------------------------------------------------------------

void Service::Patch(sel_t selector, void* arguments)
{
   Protected::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

fixed_string UnknownPortStr = "Unknown port";
fixed_string UserPortStr = "User port";
fixed_string NetworkPortStr = "Network port";

const char* Service::PortName(PortId pid) const
{
   switch(pid)
   {
   case UserPort: return UserPortStr;
   case NetworkPort: return NetworkPortStr;
   }

   return UnknownPortStr;
}

//------------------------------------------------------------------------------

fn_name Service_UnbindState = "Service.UnbindState";

void Service::UnbindState (State& state)
{
   Debug::ft(Service_UnbindState);

   states_.Erase(state);
}
}