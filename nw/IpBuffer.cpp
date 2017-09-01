//==============================================================================
//
//  IpBuffer.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "IpBuffer.h"
#include <ostream>
#include <string>
#include "Debug.h"
#include "Formatters.h"
#include "IpPort.h"
#include "IpPortRegistry.h"
#include "IpService.h"
#include "Memory.h"
#include "NwTracer.h"
#include "Restart.h"
#include "Singleton.h"
#include "SysSocket.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace NodeBase
{
//  The maximum size and the standard sizes for message buffers.  A small number
//  of fixed sizes reduces heap fragmentation.  It also leaves slack space that
//  reduces the odds of Memory::Realloc having to allocate a new buffer.
//
const size_t IpBuffer::MaxBuffSize = 8192;

const size_t nSizes = 5;

const size_t BuffSizes[nSizes + 1] =
{
   32, 128, 512, 2048, IpBuffer::MaxBuffSize
};

//------------------------------------------------------------------------------

fn_name IpBuffer_ctor1 = "IpBuffer.ctor";

IpBuffer::IpBuffer(MsgDirection dir, MsgSize header, MsgSize payload) :
   MsgBuffer(),
   buff_(nullptr),
   hdrSize_(header),
   buffSize_(header + payload),
   dir_(dir),
   external_(false),
   queued_(false)
{
   Debug::ft(IpBuffer_ctor1);

   buff_ = (byte_t*) Memory::Alloc(BuffSize(buffSize_), MemDyn);
}

//------------------------------------------------------------------------------

fn_name IpBuffer_ctor2 = "IpBuffer.ctor(copy)";

IpBuffer::IpBuffer(const IpBuffer& that) : MsgBuffer(that),
   buff_(nullptr),
   hdrSize_(that.hdrSize_),
   buffSize_(that.buffSize_),
   txAddr_(that.txAddr_),
   rxAddr_(that.rxAddr_),
   dir_(that.dir_),
   external_(that.external_),
   queued_(false)
{
   Debug::ft(IpBuffer_ctor2);

   buff_ = (byte_t*) Memory::Alloc(BuffSize(buffSize_), MemDyn);

   //  Copy the original buffer into the new one.
   //
   Memory::Copy(buff_, that.buff_, buffSize_);
}

//------------------------------------------------------------------------------

fn_name IpBuffer_dtor = "IpBuffer.dtor";

IpBuffer::~IpBuffer()
{
   Debug::ft(IpBuffer_dtor);

   if(buff_ != nullptr)
   {
      Memory::Free(buff_);
      buff_ = nullptr;
   }
}

//------------------------------------------------------------------------------

fn_name IpBuffer_AddBytes = "IpBuffer.AddBytes";

bool IpBuffer::AddBytes(const byte_t* source, MsgSize size, bool& moved)
{
   Debug::ft(IpBuffer_AddBytes);

   //  If the buffer can't hold SIZE more bytes, extend its size.
   //
   moved = false;

   if(buff_ == nullptr) return false;

   MsgSize paySize = PayloadSize();
   MsgSize newSize = hdrSize_ + paySize + size;

   if(newSize > buffSize_)
   {
      auto buff = (byte_t*) Memory::Realloc(buff_, BuffSize(newSize));
      if(buff == nullptr) return false;

      moved = (buff != buff_);
      buff_ = buff;
      buffSize_ = newSize;
   }

   //  Copy SIZE bytes into the buffer if they have been supplied.
   //
   if(source != nullptr)
   {
      Memory::Copy((buff_ + hdrSize_ + paySize), source, size);
   }

   return true;
}

//------------------------------------------------------------------------------

fn_name IpBuffer_BuffSize = "IpBuffer.BuffSize";

size_t IpBuffer::BuffSize(size_t nBytes)
{
   Debug::ft(IpBuffer_BuffSize);

   for(auto i = 0; i <= nSizes; ++i)
   {
      if(BuffSizes[i] >= nBytes) return BuffSizes[i];
   }

   Debug::SwErr(IpBuffer_BuffSize, nBytes, 0, ErrorLog);
   return 0;
}

//------------------------------------------------------------------------------

fn_name IpBuffer_Cleanup = "IpBuffer.Cleanup";

void IpBuffer::Cleanup()
{
   Debug::ft(IpBuffer_Cleanup);

   if(buff_ != nullptr)
   {
      Memory::Free(buff_);
      buff_ = nullptr;
   }

   MsgBuffer::Cleanup();
}

//------------------------------------------------------------------------------

void IpBuffer::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   MsgBuffer::Display(stream, prefix, options);

   stream << prefix << "buff     : " << strPtr(buff_) << CRLF;
   stream << prefix << "hdrSize  : " << hdrSize_ << CRLF;
   stream << prefix << "buffSize : " << buffSize_ << CRLF;
   stream << prefix << "txAddr   : " << txAddr_.to_string() << CRLF;
   stream << prefix << "rxAddr   : " << rxAddr_.to_string() << CRLF;
   stream << prefix << "dir      : " << dir_ << CRLF;
   stream << prefix << "external : " << external_ << CRLF;
   stream << prefix << "queued   : " << queued_ << CRLF;
   stream << prefix << "length   : " << PayloadSize() << CRLF;

   strBytes(stream, prefix + spaces(2), buff_, hdrSize_ + PayloadSize());
}

//------------------------------------------------------------------------------

TraceStatus IpBuffer::GetStatus() const
{
   return Singleton< NwTracer >::Instance()->BuffStatus(*this, dir_);
}

//------------------------------------------------------------------------------

fn_name IpBuffer_InvalidDiscarded = "IpBuffer.InvalidDiscarded";

void IpBuffer::InvalidDiscarded() const
{
   Debug::ft(IpBuffer_InvalidDiscarded);

   auto reg = Singleton< IpPortRegistry >::Instance();
   auto port = reg->GetPort(rxAddr_.GetPort());

   if(port != nullptr) port->InvalidDiscarded();
}

//------------------------------------------------------------------------------

fn_name IpBuffer_OutgoingBytes = "IpBuffer.OutgoingBytes";

MsgSize IpBuffer::OutgoingBytes(byte_t*& bytes) const
{
   Debug::ft(IpBuffer_OutgoingBytes);

   if(external_)
   {
      bytes = buff_ + hdrSize_;
      return PayloadSize();
   }

   bytes = buff_;
   return hdrSize_ + PayloadSize();
}

//------------------------------------------------------------------------------

void IpBuffer::Patch(sel_t selector, void* arguments)
{
   MsgBuffer::Patch(selector, arguments);
}

//------------------------------------------------------------------------------

fn_name IpBuffer_Payload = "IpBuffer.Payload";

MsgSize IpBuffer::Payload(byte_t*& bytes) const
{
   Debug::ft(IpBuffer_Payload);

   bytes = buff_;

   if(bytes == nullptr) return 0;

   bytes += hdrSize_;

   return PayloadSize();
}

//------------------------------------------------------------------------------

fn_name IpBuffer_PayloadSize = "IpBuffer.PayloadSize";

MsgSize IpBuffer::PayloadSize() const
{
   Debug::ft(IpBuffer_PayloadSize);

   return buffSize_ - hdrSize_;
}

//------------------------------------------------------------------------------

fn_name IpBuffer_Send = "IpBuffer.Send";

bool IpBuffer::Send(bool external)
{
   Debug::ft(IpBuffer_Send);

   external_ = external;

   if(buff_ == nullptr)
   {
      Debug::SwErr(IpBuffer_Send, txAddr_.GetPort(), 0);
      return false;
   }

   //  An IpBuffer can be subclassed, so truncate the outgoing message to
   //  the current size of the application payload.  This prevents unused
   //  bytes from being sent if the buffer is queued for output, at which
   //  time it gets downclassed to an IpBuffer.
   //
   buffSize_ = hdrSize_ + PayloadSize();

   //  If there is a dedicated socket for the destination, send the message
   //  over it.  If not, find the IP service associated with the sender and
   //  ask it to create a socket (e.g. for TCP).  If it doesn't, try to use
   //  the port's socket, the assumption being it is shared (e.g. for UDP).
   //
   auto socket = rxAddr_.GetSocket();

   if(socket == nullptr)
   {
      auto txPort = txAddr_.GetPort();
      auto txProto = txAddr_.GetProtocol();
      auto reg = Singleton< IpPortRegistry >::Instance();
      auto ipPort = reg->GetPort(txPort, txProto);

      if(ipPort == nullptr)
      {
         Debug::SwErr(IpBuffer_Send, txPort, 1);
         return false;
      }

      auto svc = ipPort->GetService();

      if(svc == nullptr)
      {
         Debug::SwErr(IpBuffer_Send, txPort, 2);
         return false;
      }

      socket = svc->CreateAppSocket();

      if(socket == nullptr)
      {
         socket = ipPort->GetSocket();

         if(socket == nullptr)
         {
            if(Restart::GetStatus() != ShuttingDown)
            {
               Debug::SwErr(IpBuffer_Send, txPort, 3);
            }

            return false;
         }
      }
   }

   return (socket->SendBuff(*this) != SysSocket::SendFailed);
}
}