//==============================================================================
//
//  MediaSsm.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "MediaSsm.h"
#include <ostream>
#include <string>
#include "Debug.h"
#include "MediaPsm.h"
#include "SsmContext.h"
#include "SysTypes.h"
#include "Tones.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace MediaBase
{
fn_name MediaSsm_ctor = "MediaSsm.ctor";

MediaSsm::MediaSsm(ServiceId sid) : RootServiceSM(sid),
   mgwPsm_(nullptr)
{
   Debug::ft(MediaSsm_ctor);
}

//------------------------------------------------------------------------------

fn_name MediaSsm_dtor = "MediaSsm.dtor";

MediaSsm::~MediaSsm()
{
   Debug::ft(MediaSsm_dtor);
}

//------------------------------------------------------------------------------

void MediaSsm::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   RootServiceSM::Display(stream, prefix, options);

   stream << prefix << "mgwPsm : " << mgwPsm_ << CRLF;
}

//------------------------------------------------------------------------------

fn_name MediaSsm_GetSubtended = "MediaSsm.GetSubtended";

void MediaSsm::GetSubtended(Base* objects[], size_t& count) const
{
   Debug::ft(MediaSsm_GetSubtended);

   RootServiceSM::GetSubtended(objects, count);

   if(mgwPsm_ != nullptr) mgwPsm_->GetSubtended(objects, count);
}

//------------------------------------------------------------------------------

fn_name MediaSsm_NotifyListeners = "MediaSsm.NotifyListeners";

void MediaSsm::NotifyListeners
   (const ProtocolSM& txPsm, Switch::PortId txPort) const
{
   Debug::ft(MediaSsm_NotifyListeners);

   //  Notify all of the PSMs that are listening to txPsm that they should
   //  now listen to txPort.
   //
   auto ctx = GetContext();

   for(auto psm = ctx->FirstPsm(); psm != nullptr; ctx->NextPsm(psm))
   {
      auto mpsm = static_cast< MediaPsm* >(psm);

      if((mpsm->GetOgPsm() == &txPsm) && (mpsm->GetOgTone() == Tone::Media))
      {
         mpsm->SetOgPort(txPort);
      }
   }
}

//------------------------------------------------------------------------------

fn_name MediaSsm_PsmDeleted = "MediaSsm.PsmDeleted";

void MediaSsm::PsmDeleted(ProtocolSM& exPsm)
{
   Debug::ft(MediaSsm_PsmDeleted);

   //  Notify all of the PSMs that are listening to exPsm that exPsm has
   //  been deleted.
   //
   auto ctx = GetContext();

   for(auto psm = ctx->FirstPsm(); psm != nullptr; ctx->NextPsm(psm))
   {
      auto mpsm = static_cast< MediaPsm* >(psm);

      if(mpsm->GetOgPsm() == &exPsm)
      {
         //  This often occurs at the end of a transaction, in which case
         //  ProcessOgMsg has already been invoked on MPSM if it precedes
         //  exPsm in the SSM's PSM queue.  It is then* too late* for MPSM
         //  to send its MediaInfo parameter during this transaction.  An
         //  application must invoke exPsm.SetIcTone(Tone::Silence) during
         //  the transaction to avoid this type of problem.
         //
         mpsm->SetOgPsm(nullptr);
      }
   }

   if(mgwPsm_ == &exPsm) mgwPsm_ = nullptr;

   RootServiceSM::PsmDeleted(exPsm);
}

//------------------------------------------------------------------------------

fn_name MediaSsm_SetMgwPsm = "MediaSsm.SetMgwPsm";

bool MediaSsm::SetMgwPsm(ProtocolSM* psm)
{
   Debug::ft(MediaSsm_SetMgwPsm);

   if(psm != nullptr)
   {
      if(mgwPsm_ == nullptr)
      {
         mgwPsm_ = psm;
         return true;
      }

      //  A media gateway PSM already exists.
      //
      Debug::SwErr(MediaSsm_SetMgwPsm, 0, 1);
   }
   else
   {
      if(mgwPsm_ != nullptr)
      {
         mgwPsm_ = nullptr;
         return true;
      }

      //  There wasn't a media gateway PSM to free.
      //
      Debug::SwErr(MediaSsm_SetMgwPsm, 0, 0);
   }

   return false;
}
}