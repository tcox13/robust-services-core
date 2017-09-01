//==============================================================================
//
//  SysIpL2Addr.h
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#ifndef SYSIPL2ADDR_H_INCLUDED
#define SYSIPL2ADDR_H_INCLUDED

#include "Object.h"
#include <string>
#include "NwTypes.h"

//------------------------------------------------------------------------------

namespace NodeBase
{
//  Operating system abstraction layer: layer 2 IP address.
//
class SysIpL2Addr : public Object
{
public:
   //  Constructs a nil address (all ones).
   //
   SysIpL2Addr();

   //  Constructs an IPv4 address.
   //
   explicit SysIpL2Addr(ipv4addr_t v4Addr);

   //  Constructs an IPv4 address from the string "nnn.nnn.nnn.nnn".
   //  Failure can be checked using IsValid.
   //
   explicit SysIpL2Addr(const std::string& text);

   //  Copy constructor.
   //
   SysIpL2Addr(const SysIpL2Addr& that);

   //  Copy operator.
   //
   SysIpL2Addr& operator=(const SysIpL2Addr& that);

   //  Virtual to allow subclassing.
   //
   virtual ~SysIpL2Addr();

   //  Constructs the loopback address (127.0.0.1) in host order.
   //
   static SysIpL2Addr LoopbackAddr();

   //  Returns true if the address is non-nil.
   //
   bool IsValid() const;

   //  Returns the full IPv4 address.
   //
   ipv4addr_t GetIpV4Addr() const { return v4Addr_; }

   //  Returns the address as a string ("a.b.c.d").
   //
   virtual std::string to_str() const;

   //  Updates NAME to the standard name of this host.
   //
   static bool HostName(std::string& name);

   //  Overridden to display member variables.
   //
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   virtual void Patch(sel_t selector, void* arguments) override;
protected:
   //  Sets the full IPv4 address;
   //
   void SetIpV4Addr(ipv4addr_t v4Addr) { v4Addr_ = v4Addr; }
private:
   //  IPv4 address.
   //
   ipv4addr_t v4Addr_;
};
}
#endif