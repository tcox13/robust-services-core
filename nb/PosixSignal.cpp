//==============================================================================
//
//  PosixSignal.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "PosixSignal.h"
#include <ostream>
#include <string>
#include "Algorithms.h"
#include "Debug.h"
#include "Formatters.h"
#include "PosixSignalRegistry.h"
#include "Singleton.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace NodeBase
{
fn_name PosixSignal_ctor = "PosixSignal.ctor";

PosixSignal::PosixSignal(signal_t value, const char* name,
   const char* expl, uint8_t severity, const Flags& attrs) :
   value_(value),
   name_(name),
   expl_(expl),
   severity_(severity),
   attrs_(attrs)
{
   Debug::ft(PosixSignal_ctor);

   Singleton< PosixSignalRegistry >::Instance()->BindSignal(*this);
}

//------------------------------------------------------------------------------

fn_name PosixSignal_dtor = "PosixSignal.dtor";

PosixSignal::~PosixSignal()
{
   Debug::ft(PosixSignal_dtor);

   Singleton< PosixSignalRegistry >::Instance()->UnbindSignal(*this);
}

//------------------------------------------------------------------------------

ptrdiff_t PosixSignal::CellDiff()
{
   int local;
   auto fake = reinterpret_cast< const PosixSignal* >(&local);
   return ptrdiff(&fake->sid_, fake);
}

//------------------------------------------------------------------------------

fixed_string AttrStrings[PosixSignal::Attribute_N] =
{
   "Native",
   "Break",
   "NoRecover",
   "Interrupt",
   "Delayed",
   "Exit",
   "Final",
   "NoLog",
   "NoError"
};

void PosixSignal::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Protected::Display(stream, prefix, options);

   stream << prefix << "value    : " << value_ << CRLF;
   stream << prefix << "name     : " << name_ << CRLF;
   stream << prefix << "expl     : " << expl_ << CRLF;
   stream << prefix << "severity : " << severity_ << CRLF;
   stream << prefix << "attrs    : " << " (";

   bool found = false;

   for(auto i = 0; i < Attribute_N; ++i)
   {
      if(attrs_.test(i))
      {
         if(found) stream << spaces(2);
         stream << AttrStrings[i];
         found = true;
      }
   }

   if(!found) stream << "none";
   stream << ')' << CRLF;
}

//------------------------------------------------------------------------------

void PosixSignal::Patch(sel_t selector, void* arguments)
{
   Protected::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

Flags PS_Break()
{
   return Flags(1 << PosixSignal::Break);
}

Flags PS_Delayed()
{
   return Flags(1 << PosixSignal::Delayed);
}

Flags PS_Exit()
{
   return Flags(1 << PosixSignal::Exit);
}

Flags PS_Final()
{
   return Flags(1 << PosixSignal::Final);
}

Flags PS_Interrupt()
{
   return Flags(1 << PosixSignal::Interrupt);
}

Flags PS_Native()
{
   return Flags(1 << PosixSignal::Native);
}

Flags PS_NoError()
{
   return Flags(1 << PosixSignal::NoError);
}

Flags PS_NoLog()
{
   return Flags(1 << PosixSignal::NoLog);
}

Flags PS_NoRecover()
{
   return Flags(1 << PosixSignal::NoRecover);
}
}