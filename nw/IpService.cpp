//==============================================================================
//
//  IpService.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "IpService.h"
#include <sstream>
#include <string>
#include "Algorithms.h"
#include "Debug.h"
#include "IpPort.h"
#include "IpPortRegistry.h"
#include "IpServiceRegistry.h"
#include "Log.h"
#include "Singleton.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace NodeBase
{
const id_t IpService::MaxId = 1000;

//------------------------------------------------------------------------------

fn_name IpService_ctor = "IpService.ctor";

IpService::IpService()
{
   Debug::ft(IpService_ctor);

   Singleton< IpServiceRegistry >::Instance()->BindService(*this);
}

//------------------------------------------------------------------------------

fn_name IpService_dtor = "IpService.dtor";

IpService::~IpService()
{
   Debug::ft(IpService_dtor);

   Singleton< IpServiceRegistry >::Instance()->UnbindService(*this);
}

//------------------------------------------------------------------------------

ptrdiff_t IpService::CellDiff()
{
   int local;
   auto fake = reinterpret_cast< const IpService* >(&local);
   return ptrdiff(&fake->sid_, fake);
}

//------------------------------------------------------------------------------

fn_name IpService_CreateHandler = "IpService.CreateHandler";

InputHandler* IpService::CreateHandler(IpPort* port) const
{
   Debug::ft(IpService_CreateHandler);

   Debug::SwErr(IpService_CreateHandler, Name(), port->GetPort());
   return nullptr;
}

//------------------------------------------------------------------------------

fn_name IpService_CreatePort = "IpService.CreatePort";

IpPort* IpService::CreatePort(ipport_t pid)
{
   Debug::ft(IpService_CreatePort);

   Debug::SwErr(IpService_CreatePort, Name(), pid);
   return nullptr;
}

//------------------------------------------------------------------------------

fn_name IpService_CreateText = "IpService.CreateText";

CliText* IpService::CreateText() const
{
   Debug::ft(IpService_CreateText);

   Debug::SwErr(IpService_CreateText, Name(), sid_.GetId());
   return nullptr;
}

//------------------------------------------------------------------------------

void IpService::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Protected::Display(stream, prefix, options);

   stream << prefix << "sid      : " << sid_.to_str() << CRLF;
   stream << prefix << "Name     : " << Name() << CRLF;
   stream << prefix << "Protocol : " << Protocol() << CRLF;
   stream << prefix << "Port     : " << Port() << CRLF;
   stream << prefix << "Faction  : " << GetFaction() << CRLF;
   stream << prefix << "RxSize   : " << RxSize() << CRLF;
   stream << prefix << "TxSize   : " << TxSize() << CRLF;
}

//------------------------------------------------------------------------------

fn_name IpService_GetAppSocketSizes = "IpService.GetAppSocketSizes";

void IpService::GetAppSocketSizes(size_t& rxSize, size_t& txSize) const
{
   Debug::ft(IpService_GetAppSocketSizes);

   rxSize = 0;
   txSize = 0;
   Debug::SwErr(IpService_GetAppSocketSizes, Name(), sid_.GetId());
}

//------------------------------------------------------------------------------

fn_name IpService_GetFaction = "IpService.GetFaction";

Faction IpService::GetFaction() const
{
   Debug::ft(IpService_GetFaction);

   Debug::SwErr(IpService_GetFaction, Name(), sid_.GetId());
   return OperationsFaction;
}

//------------------------------------------------------------------------------

fn_name IpService_Name = "IpService.Name";

const char* IpService::Name() const
{
   Debug::ft(IpService_Name);

   Debug::SwErr(IpService_Name, 0, sid_.GetId());
   return ERROR_STR;
}

//------------------------------------------------------------------------------

void IpService::Patch(sel_t selector, void* arguments)
{
   Protected::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

fn_name IpService_Port = "IpService.Port";

ipport_t IpService::Port() const
{
   Debug::ft(IpService_Port);

   Debug::SwErr(IpService_Port, Name(), sid_.GetId());
   return NilIpPort;
}

//------------------------------------------------------------------------------

fn_name IpService_Protocol = "IpService.Protocol";

IpProtocol IpService::Protocol() const
{
   Debug::ft(IpService_Protocol);

   Debug::SwErr(IpService_Protocol, Name(), sid_.GetId());
   return IpAny;
}

//------------------------------------------------------------------------------

fn_name IpService_Provision = "IpService.Provision";

IpPort* IpService::Provision(ipport_t pid)
{
   Debug::ft(IpService_Provision);

   auto reg = Singleton< IpPortRegistry >::Instance();
   auto port = reg->GetPort(pid, Protocol());

   if(port != nullptr)
   {
      auto svc = port->GetService();
      if(svc != this)
      {
         auto log = Log::Create("IP PORT OCCUPIED");
         if(log != nullptr)
         {
            *log << "port=" << pid << " errval=" << Name() << CRLF;
            Log::Spool(log);
         }
         return nullptr;
      }
      return port;
   }

   port = CreatePort(pid);
   if(port == nullptr)
   {
      string info(Name());
      info += " : failed to allocate IpPort";
      Debug::SwErr(IpService_Provision, info, pid);
      return nullptr;
   }

   auto handler = CreateHandler(port);
   if(handler == nullptr)
   {
      string info(Name());
      info += " : failed to allocate InputHandler";
      Debug::SwErr(IpService_Provision, info, pid);
      return nullptr;
   }

   return port;
}

//------------------------------------------------------------------------------

fn_name IpService_RxSize = "IpService.RxSize";

size_t IpService::RxSize() const
{
   Debug::ft(IpService_RxSize);

   Debug::SwErr(IpService_RxSize, Name(), sid_.GetId());
   return 0;
}

//------------------------------------------------------------------------------

fn_name IpService_Startup = "IpService.Startup";

void IpService::Startup(RestartLevel level)
{
   Debug::ft(IpService_Startup);

   auto pid = Port();
   if(pid != NilIpPort) Provision(pid);
}

//------------------------------------------------------------------------------

fn_name IpService_TxSize = "IpService.TxSize";

size_t IpService::TxSize() const
{
   Debug::ft(IpService_TxSize);

   Debug::SwErr(IpService_TxSize, Name(), sid_.GetId());
   return 0;
}
}