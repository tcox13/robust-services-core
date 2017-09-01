//==============================================================================
//
//  AnalyzeSapEvent.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "SbEvents.h"
#include <ostream>
#include <string>
#include "Context.h"
#include "Debug.h"
#include "Message.h"
#include "SbTrace.h"
#include "SysTypes.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace SessionBase
{
fn_name AnalyzeSapEvent_ctor = "AnalyzeSapEvent.ctor";

AnalyzeSapEvent::AnalyzeSapEvent(ServiceSM& owner, StateId currState,
   Event& currEvent, TriggerId tid) :
   Event(AnalyzeSap, &owner),
   currState_(currState),
   currEvent_(&currEvent),
   trigger_(tid),
   currSsm_(nullptr),
   currInit_(nullptr),
   savedMsg_(nullptr)
{
   Debug::ft(AnalyzeSapEvent_ctor);
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_dtor = "AnalyzeSapEvent.dtor";

AnalyzeSapEvent::~AnalyzeSapEvent()
{
   Debug::ft(AnalyzeSapEvent_dtor);
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_BuildSap = "AnalyzeSapEvent.BuildSap";

Event* AnalyzeSapEvent::BuildSap(ServiceSM& owner, TriggerId tid)
{
   Debug::ft(AnalyzeSapEvent_BuildSap);

   //  Second-order modifiers receive the Analyze SAP event in its
   //  original form.
   //
   return this;
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_BuildSnp = "AnalyzeSapEvent.BuildSnp";

Event* AnalyzeSapEvent::BuildSnp(ServiceSM& owner, TriggerId tid)
{
   Debug::ft(AnalyzeSapEvent_BuildSnp);

   //  Notification is not provided after handling the Analyze SAP event.
   //
   return nullptr;
}

//------------------------------------------------------------------------------

void AnalyzeSapEvent::Capture
   (ServiceId sid, const State& state, EventHandler::Rc rc) const
{
   new SxpTrace(sid, state, *this, rc);
}

//------------------------------------------------------------------------------

void AnalyzeSapEvent::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Event::Display(stream, prefix, options);

   stream << prefix << "currState : " << int(currState_) << CRLF;
   stream << prefix << "currEvent : " << currEvent_ << CRLF;
   stream << prefix << "trigger   : " << int(trigger_) << CRLF;
   stream << prefix << "currSsm   : " << currSsm_ << CRLF;
   stream << prefix << "currInit  : " << currInit_ << CRLF;
   stream << prefix << "savedMsg  : " << savedMsg_ << CRLF;
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_Free = "AnalyzeSapEvent.Free";

void AnalyzeSapEvent::Free()
{
   Debug::ft(AnalyzeSapEvent_Free);

   //  Free the underlying event and the SAP.
   //
   currEvent_->Free();
   Event::Free();
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_FreeContext = "AnalyzeSapEvent.FreeContext";

void AnalyzeSapEvent::FreeContext(bool freeMsg)
{
   Debug::ft(AnalyzeSapEvent_FreeContext);

   //  Before freeing the SAP, restore the saved message unless freeing it.
   //
   if(location_ == Saved)
   {
      if(!freeMsg) savedMsg_->Restore();
      savedMsg_->Unsave();
      savedMsg_ = nullptr;
      Free();
      return;
   }

   Debug::SwErr(AnalyzeSapEvent_FreeContext, location_, 0);
}

//------------------------------------------------------------------------------

void AnalyzeSapEvent::Patch(sel_t selector, void* arguments)
{
   Event::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_Restore = "AnalyzeSapEvent.Restore";

Event* AnalyzeSapEvent::Restore(EventHandler::Rc& rc)
{
   Debug::ft(AnalyzeSapEvent_Restore);

   //  Restore the SAP, its underlying event, and the saved context message.
   //
   if(Event::Restore(rc) != nullptr)
   {
      if(currEvent_->Restore(rc) != nullptr)
      {
         if(savedMsg_->Restore())
         {
            savedMsg_->Unsave();
            savedMsg_ = nullptr;
            return this;
         }
      }

      Context::Kill(AnalyzeSapEvent_Restore, 0, 0);
   }

   return nullptr;
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_RestoreContext = "AnalyzeSapEvent.RestoreContext";

Event* AnalyzeSapEvent::RestoreContext(EventHandler::Rc& rc)
{
   Debug::ft(AnalyzeSapEvent_RestoreContext);

   //  The Restore function handles everything.
   //
   return Restore(rc);
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_Save = "AnalyzeSapEvent.Save";

bool AnalyzeSapEvent::Save()
{
   Debug::ft(AnalyzeSapEvent_Save);

   //  Save the SAP and its underlying event.
   //
   if(Event::Save())
   {
      if(currEvent_->Save()) return true;

      EventHandler::Rc rc;
      Event::Restore(rc);
   }

   return false;
}

//------------------------------------------------------------------------------

fn_name AnalyzeSapEvent_SaveContext = "AnalyzeSapEvent.SaveContext";

bool AnalyzeSapEvent::SaveContext()
{
   Debug::ft(AnalyzeSapEvent_SaveContext);

   //  Save the context message if successful in saving the SAP event.
   //
   if(Save())
   {
      auto msg = Context::ContextMsg();

      if(msg != nullptr)
      {
         msg->Save();
         savedMsg_ = msg;
         return true;
      }

      EventHandler::Rc rc;
      Restore(rc);
   }

   return false;
}
}