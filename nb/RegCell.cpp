//==============================================================================
//
//  RegCell.cpp
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
#include "RegCell.h"
#include <iosfwd>
#include <sstream>
#include "Debug.h"

using std::string;

//------------------------------------------------------------------------------

namespace NodeBase
{
RegCell::RegCell() : id(NIL_ID), bound(false) { }

//------------------------------------------------------------------------------

fn_name RegCell_dtor = "RegCell.dtor";

RegCell::~RegCell()
{
   if(bound)
   {
      Debug::SwErr(RegCell_dtor, id, 0);
   }
}

//------------------------------------------------------------------------------

fn_name RegCell_SetId = "RegCell.SetId";

void RegCell::SetId(id_t cid)
{
   if(bound)
      Debug::SwErr(RegCell_SetId, id, cid);
   else
      id = cid;
}

//------------------------------------------------------------------------------

string RegCell::to_str() const
{
   std::ostringstream stream;
   stream << id;
   if(!bound) stream << " (not bound)";
   return stream.str();
}
}
