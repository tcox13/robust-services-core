//==============================================================================
//
//  ForceTransitionEvent.cpp
//
//  Copyright (C) 2017  Greg Utas
//
//  This file is part of the Robust Services Core (RSC).
//
//  RSC is free software: you can redistribute it and/or modify it under the
//  terms of the GNU General Public License as published by the Free Software
//  Foundation, either version 3 of the License, or (at your option) any later
//  version.
//
//  RSC is distributed in the hope that it will be useful, but WITHOUT ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
//  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
//  details.
//
//  You should have received a copy of the GNU General Public License along
//  with RSC.  If not, see <http://www.gnu.org/licenses/>.
//
#include "SbEvents.h"
#include <ostream>
#include <string>
#include "Debug.h"
#include "SysTypes.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace SessionBase
{
fn_name ForceTransitionEvent_ctor = "ForceTransitionEvent.ctor";

ForceTransitionEvent::ForceTransitionEvent
   (ServiceSM& owner, const EventHandler& handler) :
   Event(ForceTransition, &owner),
   handler_(&handler)
{
   Debug::ft(ForceTransitionEvent_ctor);
}

//------------------------------------------------------------------------------

fn_name ForceTransitionEvent_dtor = "ForceTransitionEvent.dtor";

ForceTransitionEvent::~ForceTransitionEvent()
{
   Debug::ft(ForceTransitionEvent_dtor);
}

//------------------------------------------------------------------------------

fn_name ForceTransitionEvent_BuildSap = "ForceTransitionEvent.BuildSap";

Event* ForceTransitionEvent::BuildSap(ServiceSM& owner, TriggerId tid)
{
   Debug::ft(ForceTransitionEvent_BuildSap);

   //  Modifiers cannot analyze or intercept the Force Transition event.
   //
   return nullptr;
}

//------------------------------------------------------------------------------

void ForceTransitionEvent::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Event::Display(stream, prefix, options);

   stream << prefix << "handler : " << handler_ << CRLF;
}

//------------------------------------------------------------------------------

void ForceTransitionEvent::Patch(sel_t selector, void* arguments)
{
   Event::Patch(selector, arguments);
}
}
