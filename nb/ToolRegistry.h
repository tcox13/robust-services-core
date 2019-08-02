//==============================================================================
//
//  ToolRegistry.h
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
#ifndef TOOLREGISTRY_H_INCLUDED
#define TOOLREGISTRY_H_INCLUDED

#include "Immutable.h"
#include <cstddef>
#include "NbTypes.h"
#include "Registry.h"
#include "SysTypes.h"

namespace NodeBase
{
   class Tool;
}

//------------------------------------------------------------------------------

namespace NodeBase
{
//  Global registry for Tool subclasses.
//
class ToolRegistry : public Immutable
{
   friend class Singleton< ToolRegistry >;
public:
   //  Adds TOOL to the registry.
   //
   bool BindTool(Tool& tool);

   //  Removes TOOL from the registry.
   //
   void UnbindTool(Tool& tool);

   //  Returns the tool registered against ID.
   //
   Tool* GetTool(FlagId id) const;

   //  Returns the tool, if any, whose CLI character is ABBR.
   //
   Tool* FindTool(char abbr) const;

   //  Returns the registry of tools.  Used for iteration.
   //
   const Registry< Tool >& Tools() const { return tools_; }

   //  Overridden to display member variables.
   //
   void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   void Patch(sel_t selector, void* arguments) override;
private:
   //  Private because this singleton is not subclassed.
   //
   ToolRegistry();

   //  Private because this singleton is not subclassed.
   //
   ~ToolRegistry();

   //> The maximum number of tools that can register.
   //
   static const size_t MaxTools;

   //  The global registry of CLI increments.
   //
   Registry< Tool > tools_;
};
}
#endif
