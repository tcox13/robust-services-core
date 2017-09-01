//==============================================================================
//
//  PotsBicService.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "PotsBicService.h"
#include "BcCause.h"
#include "BcSessions.h"
#include "Context.h"
#include "Debug.h"
#include "EventHandler.h"
#include "PotsFeatures.h"
#include "PotsProfile.h"
#include "PotsSessions.h"
#include "SbAppIds.h"
#include "SbEvents.h"
#include "ServiceSM.h"
#include "Singleton.h"
#include "State.h"
#include "SysTypes.h"

//------------------------------------------------------------------------------

namespace PotsBase
{
class PotsBicNull : public State
{
   friend class Singleton< PotsBicNull >;
private:
   PotsBicNull();
};

class PotsBicSsm : public ServiceSM
{
public:
   PotsBicSsm();
   ~PotsBicSsm();
private:
   virtual ServicePortId CalcPort(const AnalyzeMsgEvent& ame) override;
   virtual EventHandler::Rc ProcessInitAck
      (Event& currEvent, Event*& nextEvent) override;
   virtual EventHandler::Rc ProcessInitNack
      (Event& currEvent, Event*& nextEvent) override;
};

//==============================================================================

fn_name PotsBicInitiator_ctor = "PotsBicInitiator.ctor";

PotsBicInitiator::PotsBicInitiator() : Initiator(PotsBicServiceId,
   PotsCallServiceId, BcTrigger::AuthorizeTerminationSap,
   PotsAuthorizeTerminationSap::PotsBicPriority)
{
   Debug::ft(PotsBicInitiator_ctor);
}

//------------------------------------------------------------------------------

fn_name PotsBicInitiator_ProcessEvent = "PotsBicInitiator.ProcessEvent";

EventHandler::Rc PotsBicInitiator::ProcessEvent
   (const ServiceSM& parentSsm, Event& icEvent, Event*& ogEvent) const
{
   Debug::ft(PotsBicInitiator_ProcessEvent);

   auto& pssm = static_cast< const PotsBcSsm& >(parentSsm);
   auto prof = pssm.Profile();

   if(prof->HasFeature(BIC))
   {
      ogEvent = new InitiationReqEvent(*icEvent.Owner(), PotsBicServiceId);
      return EventHandler::Initiate;
   }

   return EventHandler::Pass;
}

//==============================================================================

fn_name PotsBicService_ctor = "PotsBicService.ctor";

PotsBicService::PotsBicService() : Service(PotsBicServiceId, false, true)
{
   Debug::ft(PotsBicService_ctor);

   Singleton< PotsBicNull >::Instance();
}

//------------------------------------------------------------------------------

fn_name PotsBicService_dtor = "PotsBicService.dtor";

PotsBicService::~PotsBicService()
{
   Debug::ft(PotsBicService_dtor);
}

//------------------------------------------------------------------------------

fn_name PotsBicService_AllocModifier = "PotsBicService.AllocModifier";

ServiceSM* PotsBicService::AllocModifier() const
{
   Debug::ft(PotsBicService_AllocModifier);

   return new PotsBicSsm;
}

//==============================================================================

fn_name PotsBicNull_ctor = "PotsBicNull.ctor";

PotsBicNull::PotsBicNull() : State(PotsBicServiceId, ServiceSM::Null)
{
   Debug::ft(PotsBicNull_ctor);
}

//==============================================================================

fn_name PotsBicSsm_ctor = "PotsBicSsm.ctor";

PotsBicSsm::PotsBicSsm() : ServiceSM(PotsBicServiceId)
{
   Debug::ft(PotsBicSsm_ctor);
}

//------------------------------------------------------------------------------

fn_name PotsBicSsm_dtor = "PotsBicSsm.dtor";

PotsBicSsm::~PotsBicSsm()
{
   Debug::ft(PotsBicSsm_dtor);
}

//------------------------------------------------------------------------------

fn_name PotsBicSsm_CalcPort = "PotsBicSsm.CalcPort";

ServicePortId PotsBicSsm::CalcPort(const AnalyzeMsgEvent& ame)
{
   Debug::ft(PotsBicSsm_CalcPort);

   return Parent()->CalcPort(ame);
}

//------------------------------------------------------------------------------

fn_name PotsBicSsm_ProcessInitAck = "PotsBicSsm.ProcessInitAck";

EventHandler::Rc PotsBicSsm::ProcessInitAck
   (Event& currEvent, Event*& nextEvent)
{
   Debug::ft(PotsBicSsm_ProcessInitAck);

   auto& pssm = *Parent();
   auto stid = pssm.CurrState();

   if(stid == BcState::AuthorizingTermination)
   {
      pssm.SetNextSap(BcTrigger::TerminationDeniedSap);
      nextEvent = new BcTerminationDeniedEvent
         (pssm, Cause::IncomingCallsBarred);
      return EventHandler::Revert;
   }

   Context::Kill(PotsBicSsm_ProcessInitAck, stid, 0);
   return EventHandler::Suspend;
}

//------------------------------------------------------------------------------

fn_name PotsBicSsm_ProcessInitNack = "PotsBicSsm.ProcessInitNack";

EventHandler::Rc PotsBicSsm::ProcessInitNack
   (Event& currEvent, Event*& nextEvent)
{
   Debug::ft(PotsBicSsm_ProcessInitNack);

   return EventHandler::Resume;
}
}