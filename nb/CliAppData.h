//==============================================================================
//
//  CliAppData.h
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#ifndef CLIAPPDATA_H_INCLUDED
#define CLIAPPDATA_H_INCLUDED

#include "Temporary.h"
#include <memory>

namespace NodeBase
{
   class CliThread;
}

//------------------------------------------------------------------------------

namespace NodeBase
{
//  Virtual base class for adding application-specific data to the CLI thread.
//
class CliAppData : public Temporary
{
   friend std::unique_ptr< CliAppData >::deleter_type;
public:
   //  Identifier for an application that associates data with a CLI thread.
   //  NIL_ID is used as a valid identifier.
   //
   typedef int Id;

   //> Highest valid CLI application identifier.
   //
   static const Id MaxId = 7;

   //  Events of interest to applications.
   //
   enum Event
   {
      EndOfTest
   };

   //  Returns the CLI thread associated with the data.
   //
   CliThread* Cli() const { return cli_; }

   //  Notifies the application that an event has occurred.  The default
   //  version does nothing.
   //
   virtual void EventOccurred(Event evt);

   //  Overridden to display member variables.
   //
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   //  Overridden for patching.
   //
   virtual void Patch(sel_t selector, void* arguments) override;
protected:
   //  Registers the data with CLI against ID.  Protected because this
   //  class is virtual.
   //
   CliAppData(CliThread& cli, Id id);

   //  Protected to restrict deletion to CliThread.  Virtual to allow
   //  subclassing.
   //
   virtual ~CliAppData();
private:
   //  The CLI thread associated with the data.
   //
   CliThread* cli_;

   //  The application associated with the data.
   //
   Id id_;
};

//------------------------------------------------------------------------------
//
//  Applications that register data with a CliThread.
//
enum CliAppIds
{
   TestcaseAppId,
   TestSessionAppId
};
}
#endif