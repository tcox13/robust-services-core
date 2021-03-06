//==============================================================================
//
//  Immutable.cpp
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
#include "Immutable.h"
#include "Debug.h"
#include "Memory.h"

//------------------------------------------------------------------------------

namespace NodeBase
{
fn_name Immutable_ctor = "Immutable.ctor";

Immutable::Immutable()
{
   Debug::ft(Immutable_ctor);
}

//------------------------------------------------------------------------------

fn_name Immutable_delete1 = "Immutable.operator delete";

void Immutable::operator delete(void* addr)
{
   Debug::ftnt(Immutable_delete1);

   Memory::Free(addr, MemImmutable);
}

//------------------------------------------------------------------------------

fn_name Immutable_delete2 = "Immutable.operator delete[]";

void Immutable::operator delete[](void* addr)
{
   Debug::ftnt(Immutable_delete2);

   Memory::Free(addr, MemImmutable);
}

//------------------------------------------------------------------------------

fn_name Immutable_delete3 = "Immutable.operator delete(place)";

void Immutable::operator delete(void* addr, void* place) noexcept
{
   Debug::ftnt(Immutable_delete3);
}

//------------------------------------------------------------------------------

fn_name Immutable_delete4 = "Immutable.operator delete[](place)";

void Immutable::operator delete[](void* addr, void* place) noexcept
{
   Debug::ftnt(Immutable_delete4);
}

//------------------------------------------------------------------------------

fn_name Immutable_new1 = "Immutable.operator new";

void* Immutable::operator new(size_t size)
{
   Debug::ft(Immutable_new1);

   return Memory::Alloc(size, MemImmutable);
}

//------------------------------------------------------------------------------

fn_name Immutable_new2 = "Immutable.operator new[]";

void* Immutable::operator new[](size_t size)
{
   Debug::ft(Immutable_new2);

   return Memory::Alloc(size, MemImmutable);
}

//------------------------------------------------------------------------------

fn_name Immutable_new3 = "Immutable.operator new(place)";

void* Immutable::operator new(size_t size, void* place)
{
   Debug::ft(Immutable_new3);

   return place;
}

//------------------------------------------------------------------------------

fn_name Immutable_new4 = "Immutable.operator new[](place)";

void* Immutable::operator new[](size_t size, void* place)
{
   Debug::ft(Immutable_new4);

   return place;
}

//------------------------------------------------------------------------------

void Immutable::Patch(sel_t selector, void* arguments)
{
   Object::Patch(selector, arguments);
}
}