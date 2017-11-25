//==============================================================================
//
//  PosixSignalRegistry.h
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
#ifndef POSIXSIGNALREGISTRY_H_INCLUDED
#define POSIXSIGNALREGISTRY_H_INCLUDED

#include "Protected.h"
#include <string>
#include "NbTypes.h"
#include "Registry.h"
#include "SysTypes.h"

namespace NodeBase
{
   class PosixSignal;
}

//------------------------------------------------------------------------------

namespace NodeBase
{
//  Global registry for POSIX signals.
//
class PosixSignalRegistry : public Protected
{
   friend class Singleton< PosixSignalRegistry >;
   friend class PosixSignal;
public:
   //  Returns the signal identified by VALUE.
   //
   PosixSignal* Find(signal_t value) const;

   //  Returns the signal identified by NAME.
   //
   PosixSignal* Find(const std::string& name) const;

   //  Returns a string containing VALUE and the signal's name and explanation.
   //
   std::string strSignal(signal_t value) const;

   //  Returns the attributes for the signal identified by VALUE.
   //
   Flags Attrs(signal_t value) const;

   //  Returns the signal defined by NAME if it can be thrown on this platform.
   //  Returns SIGNIL if the signal cannot be thrown.
   //
   signal_t Value(const std::string& name) const;

   //  Returns the registry of signals.  Used for iteration.
   //
   const Registry< PosixSignal >& Signals() const { return signals_; }

   //  Overridden to display member variables.
   //
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   virtual void Patch(sel_t selector, void* arguments) override;
private:
   //  Private because this singleton is not subclassed.
   //
   PosixSignalRegistry();

   //  Private because this singleton is not subclassed.
   //
   ~PosixSignalRegistry();

   //  Adds SIGNAL to the registry.
   //
   bool BindSignal(PosixSignal& signal);

   //  Removes SIGNAL from the registry.
   //
   void UnbindSignal(PosixSignal& signal);

   //  The global registry of POSIX signals.
   //
   Registry< PosixSignal > signals_;
};
}
#endif
