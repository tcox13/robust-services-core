//==============================================================================
//
//  NtIncrement.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "NtIncrement.h"
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <memory>
#include <sstream>
#include "Algorithms.h"
#include "Class.h"
#include "CliBoolParm.h"
#include "CliCommandSet.h"
#include "CliIntParm.h"
#include "CliText.h"
#include "CliThread.h"
#include "Clock.h"
#include "Debug.h"
#include "Element.h"
#include "Formatters.h"
#include "FunctionGuard.h"
#include "FunctionProfiler.h"
#include "FunctionTrace.h"
#include "LeakyBucketCounter.h"
#include "Module.h"
#include "MsgBuffer.h"
#include "NbAppIds.h"
#include "NbCliParms.h"
#include "NbSignals.h"
#include "NtTestData.h"
#include "ObjectPool.h"
#include "ObjectPoolRegistry.h"
#include "PosixSignal.h"
#include "PosixSignalRegistry.h"
#include "Q1Link.h"
#include "Q1Way.h"
#include "Q2Link.h"
#include "Q2Way.h"
#include "RegCell.h"
#include "Registry.h"
#include "Singleton.h"
#include "SysThread.h"
#include "SysTime.h"
#include "Temporary.h"
#include "ThisThread.h"
#include "Thread.h"
#include "ToolTypes.h"

using std::ostream;
using std::string;

//------------------------------------------------------------------------------

namespace NodeTools
{
//  The CORRUPT command.
//
class FreeqOffsetParm : public CliIntParm
{
public: FreeqOffsetParm();
};

class ObjectPoolText : public CliText
{
public: ObjectPoolText();
};

fixed_string FreeqOffsetExpl = "offset into free queue (0 = head)";

FreeqOffsetParm::FreeqOffsetParm() : CliIntParm(FreeqOffsetExpl, 0, 1024) { }

fixed_string ObjectPoolTextStr = "pool";
fixed_string ObjectPoolTextExpl = "object pool";

ObjectPoolText::ObjectPoolText() :
   CliText(ObjectPoolTextExpl, ObjectPoolTextStr)
{
   BindParm(*new ObjPoolIdMandParm);
   BindParm(*new FreeqOffsetParm);
}

fixed_string CorruptWhatExpl = "what to corrupt...";

CorruptWhatParm::CorruptWhatParm() : CliTextParm(CorruptWhatExpl)
{
   BindText(*new ObjectPoolText, CorruptCommand::PoolIndex);
}

fixed_string CorruptStr = "corrupt";
fixed_string CorruptExpl = "Corrupts a data structure for testing purposes.";

CorruptCommand::CorruptCommand(bool bind) :
   CliCommand(CorruptStr, CorruptExpl)
{
   if(bind) BindParm(*new CorruptWhatParm);
}

fn_name CorruptCommand_ProcessCommand = "CorruptCommand.ProcessCommand";

word CorruptCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(CorruptCommand_ProcessCommand);

   id_t corruptWhatIndex;

   if(!Element::RunningInLab()) return cli.Report(-5, NotInFieldExpl);
   if(!GetTextIndex(corruptWhatIndex, cli)) return -1;
   return ProcessSubcommand(cli, corruptWhatIndex);
}

fn_name CorruptCommand_ProcessSubcommand = "CorruptCommand.ProcessSubcommand";

word CorruptCommand::ProcessSubcommand(CliThread& cli, id_t index) const
{
   Debug::ft(CorruptCommand_ProcessSubcommand);

   if(index != PoolIndex) return CliCommand::ProcessSubcommand(cli, index);

   word pid, n;

   if(!GetIntParm(pid, cli)) return -1;
   if(!GetIntParm(n, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< ObjectPoolRegistry >::Instance()->Pool(pid);
   if(pool == nullptr) return cli.Report(-2, NoPoolExpl);
   if(!pool->Corrupt(n)) return cli.Report(-3, EndOfFreeQueue);
   return cli.Report(0, SuccessExpl);
}

//------------------------------------------------------------------------------
//
//  The SAVE command.
//
class FuncsSortByCalls : public CliText
{
public: FuncsSortByCalls();
};

class FuncsSortByTimes : public CliText
{
public: FuncsSortByTimes();
};

class FuncsSortByNames : public CliText
{
public: FuncsSortByNames();
};

class FuncsSortHowParm : public CliTextParm
{
public: FuncsSortHowParm();
};

class FuncsText : public CliText
{
public: FuncsText();
};

fixed_string FuncsSortByCallsTextStr = "calls";
fixed_string FuncsSortByCallsTextExpl = "by number of invocations";

FuncsSortByCalls::FuncsSortByCalls() :
   CliText(FuncsSortByCallsTextExpl, FuncsSortByCallsTextStr) { }

fixed_string FuncsSortByTimesTextStr = "times";
fixed_string FuncsSortByTimesTextExpl = "by net time in function";

FuncsSortByTimes::FuncsSortByTimes() :
   CliText(FuncsSortByTimesTextExpl, FuncsSortByTimesTextStr) { }

fixed_string FuncsSortByNamesTextStr = "names";
fixed_string FuncsSortByNamesTextExpl = "by function name";

FuncsSortByNames::FuncsSortByNames() :
   CliText(FuncsSortByNamesTextExpl, FuncsSortByNamesTextStr) { }

fixed_string FuncsSortHowExpl = "how to sort (default=calls)";

const id_t SortByCallsIndex = 1;
const id_t SortByTimesIndex = 2;
const id_t SortByNamesIndex = 3;

FuncsSortHowParm::FuncsSortHowParm() : CliTextParm(FuncsSortHowExpl, true)
{
   BindText(*new FuncsSortByCalls, SortByCallsIndex);
   BindText(*new FuncsSortByTimes, SortByTimesIndex);
   BindText(*new FuncsSortByNames, SortByNamesIndex);
}

fixed_string FuncsTextStr = "funcs";
fixed_string FuncsTextExpl = "function call statistics";

FuncsText::FuncsText() : CliText(FuncsTextExpl, FuncsTextStr)
{
   BindParm(*new FileMandParm);
   BindParm(*new FuncsSortHowParm);
}

NtSaveWhatParm::NtSaveWhatParm()
{
   BindText(*new FuncsText, NtSaveCommand::FuncsIndex);
}

NtSaveCommand::NtSaveCommand(bool bind) : SaveCommand(false)
{
   if(bind) BindParm(*new NtSaveWhatParm);
}

fn_name NtSaveCommand_ProcessSubcommand = "NtSaveCommand.ProcessSubcommand";

word NtSaveCommand::ProcessSubcommand(CliThread& cli, id_t index) const
{
   Debug::ft(NtSaveCommand_ProcessSubcommand);

   if(index != FuncsIndex) return SaveCommand::ProcessSubcommand(cli, index);

   TraceRc rc;
   string title;
   id_t sortHowIndex;
   auto sort = FunctionProfiler::ByCalls;

   if(!GetFileName(title, cli)) return -1;
   if(GetTextIndexRc(sortHowIndex, cli) == Ok)
   {
      switch(sortHowIndex)
      {
      case SortByCallsIndex: sort = FunctionProfiler::ByCalls; break;
      case SortByTimesIndex: sort = FunctionProfiler::ByTimes; break;
      case SortByNamesIndex: sort = FunctionProfiler::ByNames; break;
      }
   }
   cli.EndOfInput(false);

   auto stream = cli.FileStream();
   if(stream == nullptr) return cli.Report(-7, CreateStreamFailure);

   auto yield = cli.GenerateReportPreemptably();
   FunctionGuard guard(FunctionGuard::MakePreemptable, yield);

   FunctionTrace::Postprocess();
   auto fp = std::unique_ptr< FunctionProfiler >(new FunctionProfiler);
   if(fp == nullptr) return cli.Report(-7, AllocationError);
   rc = fp->Generate(*stream, sort);
   fp.reset();

   if(rc == TraceOk)
   {
      title += ".funcs.txt";
      cli.SendToFile(title, true);
   }

   return ExplainTraceRc(cli, rc);
}

//------------------------------------------------------------------------------
//
//  The SET command.
//
class FuncScopeFullTrace : public CliText
{
public: FuncScopeFullTrace();
};

class FuncScopeCountsOnly : public CliText
{
public: FuncScopeCountsOnly();
};

class FuncScopeParm : public CliTextParm
{
public: FuncScopeParm();
};

class ScopeText : public CliText
{
public: ScopeText();
};

fixed_string FuncScopeFullTraceTextStr = "full";
fixed_string FuncScopeFullTraceTextExpl = "full trace of invocations";

FuncScopeFullTrace::FuncScopeFullTrace() :
   CliText(FuncScopeFullTraceTextExpl, FuncScopeFullTraceTextStr) { }

fixed_string FuncScopeCountsOnlyTextStr = "counts";
fixed_string FuncScopeCountsOnlyTextExpl = "count invocations per function";

FuncScopeCountsOnly::FuncScopeCountsOnly() :
   CliText(FuncScopeCountsOnlyTextExpl, FuncScopeCountsOnlyTextStr) { }

fixed_string FuncScopeExpl = "how to trace function invocations";

const id_t FuncScopeFullTraceIndex  = 1;
const id_t FuncScopeCountsOnlyIndex = 2;

FuncScopeParm::FuncScopeParm() : CliTextParm(FuncScopeExpl)
{
   BindText(*new FuncScopeFullTrace, FuncScopeFullTraceIndex);
   BindText(*new FuncScopeCountsOnly, FuncScopeCountsOnlyIndex);
}

fixed_string ScopeTextStr = "scope";
fixed_string ScopeTextExpl = "scope for function tracing";

ScopeText::ScopeText() : CliText(ScopeTextExpl, ScopeTextStr)
{
   BindParm(*new FuncScopeParm);
}

NtSetWhatParm::NtSetWhatParm()
{
   BindText(*new ScopeText, NtSetCommand::FuncTraceScope);
}

NtSetCommand::NtSetCommand(bool bind) : SetCommand(false)
{
   if(bind) BindParm(*new NtSetWhatParm);
}

fn_name NtSetCommand_ProcessSubcommand = "NtSetCommand.ProcessSubcommand";

word NtSetCommand::ProcessSubcommand(CliThread& cli, id_t index) const
{
   Debug::ft(NtSetCommand_ProcessSubcommand);

   if(index != FuncTraceScope) return SetCommand::ProcessSubcommand(cli, index);

   auto rc = TraceOk;
   id_t scope;

   if(!GetTextIndex(scope, cli)) return -1;
   cli.EndOfInput(false);

   switch(scope)
   {
   case FuncScopeFullTraceIndex:
      rc = FunctionTrace::SetScope(FunctionTrace::FullTrace);
      break;
   case FuncScopeCountsOnlyIndex:
      rc = FunctionTrace::SetScope(FunctionTrace::CountsOnly);
      break;
   default:
      return cli.Report(scope, SystemErrorExpl);
   }

   return ExplainTraceRc(cli, rc);
}

//------------------------------------------------------------------------------
//
//  The SIZES command.
//
class SizesParm : public CliBoolParm
{
public: SizesParm();
};

fixed_string SizesParmExpl = "display sizes in base classes? (default=f)";

SizesParm::SizesParm() : CliBoolParm(SizesParmExpl, true) { }

fixed_string SizesStr = "sizes";
fixed_string SizesExpl = "Displays class sizes.";

SizesCommand::SizesCommand() : CliCommand(SizesStr, SizesExpl)
{
   BindParm(*new SizesParm);
}

void SizesCommand::DisplaySizes(CliThread& cli, bool all) const
{
   *cli.obuf << "  Class = " << sizeof(Class) << CRLF;
   *cli.obuf << "  CliBoolParm = " << sizeof(CliBoolParm) << CRLF;
   *cli.obuf << "  CliIntParm = " << sizeof(CliIntParm) << CRLF;
   *cli.obuf << "  CliText = " << sizeof(CliText) << CRLF;
   *cli.obuf << "  CliTextParm = " << sizeof(CliTextParm) << CRLF;
   *cli.obuf << "  FunctionTrace = " << sizeof(FunctionTrace) << CRLF;
   *cli.obuf << "  MsgBuffer = " << sizeof(MsgBuffer) << CRLF;
   *cli.obuf << "  Module = " << sizeof(Module) << CRLF;
   *cli.obuf << "  Object = " << sizeof(Object) << CRLF;
   *cli.obuf << "  Pooled = " << sizeof(Pooled) << CRLF;
   *cli.obuf << "  Q1Link = " << sizeof(Q1Link) << CRLF;
   *cli.obuf << "  Q1Way = " << sizeof(Q1Way< Object >) << CRLF;
   *cli.obuf << "  Q2Link = " << sizeof(Q2Link) << CRLF;
   *cli.obuf << "  Q2Way = " << sizeof(Q2Way< Object >) << CRLF;
   *cli.obuf << "  RegCell = " << sizeof(RegCell) << CRLF;
   *cli.obuf << "  Registry = " << sizeof(Registry< Object >) << CRLF;
   *cli.obuf << "  SysThread = " << sizeof(SysThread) << CRLF;
   *cli.obuf << "  Thread = " << sizeof(Thread) << CRLF;
   *cli.obuf << "  TraceRecord = " << sizeof(TraceRecord) << CRLF;
}

fn_name SizesCommand_ProcessCommand = "SizesCommand.ProcessCommand";

word SizesCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(SizesCommand_ProcessCommand);

   bool all = false;

   if(GetBoolParmRc(all, cli) == Error) return -1;
   cli.EndOfInput(false);
   *cli.obuf << spaces(2) << SizesHeader << CRLF;
   DisplaySizes(cli, all);
   return 0;
}

//------------------------------------------------------------------------------
//
//  The SWFLAGS command.
//
class FlagIdParm : public CliIntParm
{
public: FlagIdParm();
};

class FlagsSetText : public CliText
{
public: FlagsSetText();
};

class FlagsClearText : public CliText
{
public: FlagsClearText();
};

class FlagsQueryText : public CliText
{
public: FlagsQueryText();
};

class FlagsAction : public CliTextParm
{
public: FlagsAction();
};

class SwFlagsCommand : public CliCommand
{
public:
   static const id_t FlagsSetIndex = 1;
   static const id_t FlagsClearIndex = 2;
   static const id_t FlagsQueryIndex = 3;

   SwFlagsCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string FlagIdExpl = "flag identifier";

FlagIdParm::FlagIdParm() : CliIntParm(FlagIdExpl, 0, MaxFlagId) { }

fixed_string FlagsSetTextStr = "set";
fixed_string FlagsSetTextExpl = "modifies a flag's setting";

FlagsSetText::FlagsSetText() : CliText(FlagsSetTextExpl, FlagsSetTextStr)
{
   BindParm(*new FlagIdParm);
   BindParm(*new SetHowParm);
}

fixed_string FlagsClearTextStr = "clear";
fixed_string FlagsClearTextExpl = "clears all flags";

FlagsClearText::FlagsClearText() :
   CliText(FlagsClearTextExpl, FlagsClearTextStr) { }

fixed_string FlagsQueryTextStr = "query";
fixed_string FlagsQueryTextExpl = "lists flags that are on";

FlagsQueryText::FlagsQueryText() :
   CliText(FlagsQueryTextExpl, FlagsQueryTextStr) { }

fixed_string FlagsActionExpl = "subcommand...";

FlagsAction::FlagsAction() : CliTextParm(FlagsActionExpl)
{
   BindText(*new FlagsSetText, SwFlagsCommand::FlagsSetIndex);
   BindText(*new FlagsClearText, SwFlagsCommand::FlagsClearIndex);
   BindText(*new FlagsQueryText, SwFlagsCommand::FlagsQueryIndex);
}

fixed_string SwFlagsStr = "swflags";
fixed_string SwFlagsExpl = "Supports flags used to control branching";

SwFlagsCommand::SwFlagsCommand() : CliCommand(SwFlagsStr, SwFlagsExpl)
{
   BindParm(*new FlagsAction);
}

fn_name SwFlagsCommand_ProcessCommand = "SwFlagsCommand.ProcessCommand";

word SwFlagsCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(SwFlagsCommand_ProcessCommand);

   id_t index, setHowIndex;
   word flag;
   Flags flags;

   if(!GetTextIndex(index, cli)) return -1;

   switch(index)
   {
   case FlagsSetIndex:
      if(!GetIntParm(flag, cli)) return -1;
      if(!GetTextIndex(setHowIndex, cli)) return -1;
      cli.EndOfInput(false);
      Debug::SetSwFlag(flag, (setHowIndex == SetHowParm::On));
      break;

   case FlagsClearIndex:
      cli.EndOfInput(false);
      Debug::ResetSwFlags();
      break;

   case FlagsQueryIndex:
      cli.EndOfInput(false);
      flags = Debug::GetSwFlags();
      *cli.obuf << spaces(2) << "Flags on (bit offsets):";

      if(flags.none())
      {
         *cli.obuf << " none";
      }
      else
      {
         for(auto i = 0; i <= MaxFlagId; ++i)
         {
            if(flags.test(i)) *cli.obuf << SPACE << i;
         }
      }

      *cli.obuf << CRLF;
      return 0;

   default:
      return cli.Report(index, SystemErrorExpl);
   }

   return cli.Report(0, SuccessExpl);
}

//------------------------------------------------------------------------------
//
//  The TESTCASE command.
//
class TestPrologParm : public CliTextParm
{
public: TestPrologParm();
};

class TestPrologText : public CliText
{
public: TestPrologText();
};

class TestEpilogParm : public CliTextParm
{
public: TestEpilogParm();
};

class TestEpilogText : public CliText
{
public: TestEpilogText();
};

class TestRecoverParm : public CliTextParm
{
public: TestRecoverParm();
};

class TestRecoverText : public CliText
{
public: TestRecoverText();
};

class TestBeginParm : public CliTextParm
{
public: TestBeginParm();
};

class TestBeginText : public CliText
{
public: TestBeginText();
};

class TestEndText : public CliText
{
public: TestEndText();
};

class TestFailCodeParm : public CliIntParm
{
public: TestFailCodeParm();
};

class TestFailExplParm : public CliTextParm
{
public: TestFailExplParm();
};

class TestFailedText : public CliText
{
public: TestFailedText();
};

class TestQueryText : public CliText
{
public: TestQueryText();
};

class TestResetText : public CliText
{
public: TestResetText();
};

fixed_string TestPrologExpl = "filename (none if omitted)";

TestPrologParm::TestPrologParm() : CliTextParm(TestPrologExpl, true) { }

fixed_string TestPrologTextStr = "prolog";
fixed_string TestPrologTextExpl = "file to read before executing a testcase";

TestPrologText::TestPrologText() :
   CliText(TestPrologTextExpl, TestPrologTextStr)
{
   BindParm(*new TestPrologParm);
}

fixed_string TestEpilogExpl = "filename (none if omitted)";

TestEpilogParm::TestEpilogParm() : CliTextParm(TestEpilogExpl, true) { }

fixed_string TestEpilogTextStr = "epilog";
fixed_string TestEpilogTextExpl = "file to read after a testcase passes";

TestEpilogText::TestEpilogText() :
   CliText(TestEpilogTextExpl, TestEpilogTextStr)
{
   BindParm(*new TestEpilogParm);
}

fixed_string TestRecoverExpl = "filename (epilog if omitted)";

TestRecoverParm::TestRecoverParm() : CliTextParm(TestRecoverExpl, true) { }

fixed_string TestRecoverTextStr = "recover";
fixed_string TestRecoverTextExpl = "file to read after a testcase fails";

TestRecoverText::TestRecoverText() :
   CliText(TestRecoverTextExpl, TestRecoverTextStr)
{
   BindParm(*new TestRecoverParm);
}

fixed_string TestBeginExpl = "testcase filename";

TestBeginParm::TestBeginParm() : CliTextParm(TestBeginExpl) { }

fixed_string TestBeginTextStr = "begin";
fixed_string TestBeginTextExpl =
   "executes a testcase (and concludes any previous one)";

TestBeginText::TestBeginText() : CliText(TestBeginTextExpl, TestBeginTextStr)
{
   BindParm(*new TestBeginParm);
}

fixed_string TestEndTextStr = "end";
fixed_string TestEndTextExpl = "concludes a testcase";

TestEndText::TestEndText() : CliText(TestEndTextExpl, TestEndTextStr) { }

fixed_string TestFailCodeExpl = "failure code";

TestFailCodeParm::TestFailCodeParm() :
   CliIntParm(TestFailCodeExpl, WORD_MIN, WORD_MAX) { }

fixed_string TestFailExpl = "explanation for failure";

TestFailExplParm::TestFailExplParm() : CliTextParm(TestFailExpl, true) { }

fixed_string TestFailedTextStr = "failed";
fixed_string TestFailedTextExpl = "records that the current testcase failed";

TestFailedText::TestFailedText() :
   CliText(TestFailedTextExpl, TestFailedTextStr)
{
   BindParm(*new TestFailCodeParm);
   BindParm(*new TestFailExplParm);
}

fixed_string TestQueryTextStr = "query";
fixed_string TestQueryTextExpl = "shows the counts of passed/failed testcases";

TestQueryText::TestQueryText() :
   CliText(TestQueryTextExpl, TestQueryTextStr) { }

fixed_string TestResetTextStr = "reset";
fixed_string TestResetTextExpl = "clears the counts of passed/failed testcases";

TestResetText::TestResetText() :
   CliText(TestResetTextExpl, TestResetTextStr) { }

fixed_string TestcaseActionExpl = "subcommand...";

TestcaseAction::TestcaseAction() : CliTextParm(TestcaseActionExpl)
{
   BindText(*new TestPrologText, TestcaseCommand::TestPrologIndex);
   BindText(*new TestEpilogText, TestcaseCommand::TestEpilogIndex);
   BindText(*new TestRecoverText, TestcaseCommand::TestRecoverIndex);
   BindText(*new TestBeginText, TestcaseCommand::TestBeginIndex);
   BindText(*new TestEndText, TestcaseCommand::TestEndIndex);
   BindText(*new TestFailedText, TestcaseCommand::TestFailedIndex);
   BindText(*new TestQueryText, TestcaseCommand::TestQueryIndex);
   BindText(*new TestResetText, TestcaseCommand::TestResetIndex);
}

fixed_string TestcaseStr = "testcase";
fixed_string TestcaseExpl = "Configures or executes testcases.";

TestcaseCommand::TestcaseCommand(bool bind) :
   CliCommand(TestcaseStr, TestcaseExpl)
{
   if(bind) BindParm(*new TestcaseAction);
}

fn_name TestcaseCommand_ConcludeTest = "TestcaseCommand.ConcludeTest";

void TestcaseCommand::ConcludeTest(CliThread& cli) const
{
   Debug::ft(TestcaseCommand_ConcludeTest);

   auto test = NtTestData::Access(cli);

   auto prev = test->Name();
   if(prev.empty()) return;

   if(test->HasFailed())
   {
      auto recover = test->Recover();
      if(!recover.empty())
      {
         auto command = "read " + recover;
         cli.Execute(command);
      }
      else
      {
         auto epilog = test->Epilog();
         if(!epilog.empty())
         {
            auto command = "read " + epilog;
            cli.Execute(command);
         }
      }
      test->IncrFailCount();
   }
   else
   {
      auto epilog = test->Epilog();
      if(!epilog.empty())
      {
         auto command = "read " + epilog;
         cli.Execute(command);
      }
      test->IncrPassCount();
   }

   test->SetName(EMPTY_STR);
   cli.Notify(CliAppData::EndOfTest);
}

fn_name TestcaseCommand_InitiateTest = "TestcaseCommand.InitiateTest";

void TestcaseCommand::InitiateTest(CliThread& cli, const string& curr) const
{
   Debug::ft(TestcaseCommand_InitiateTest);

   auto test = NtTestData::Access(cli);

   test->SetName(curr);
   test->ResetFailed();

   auto command = "symbols set testcase.name " + curr;
   cli.Execute(command);

   auto prolog = test->Prolog();

   if(!prolog.empty())
   {
      command = "read " + prolog;
      cli.Execute(command);
   }
}

fn_name TestcaseCommand_ProcessCommand = "TestcaseCommand.ProcessCommand";

word TestcaseCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TestcaseCommand_ProcessCommand);

   id_t index;

   if(!GetTextIndex(index, cli)) return -1;

   return ProcessSubcommand(cli, index);
}

fn_name TestcaseCommand_ProcessSubcommand = "TestcaseCommand.ProcessSubcommand";

word TestcaseCommand::ProcessSubcommand(CliThread& cli, id_t index) const
{
   Debug::ft(TestcaseCommand_ProcessSubcommand);

   auto test = NtTestData::Access(cli);
   if(test == nullptr) return cli.Report(-7, CreateStreamFailure);

   word rc;
   string text;

   switch(index)
   {
   case TestPrologIndex:
      if(!GetString(text, cli)) text.clear();
      cli.EndOfInput(false);
      test->SetProlog(text);
      break;

   case TestEpilogIndex:
      if(!GetString(text, cli)) text.clear();
      cli.EndOfInput(false);
      test->SetEpilog(text);
      break;

   case TestRecoverIndex:
      if(!GetString(text, cli)) text.clear();
      cli.EndOfInput(false);
      test->SetRecover(text);
      break;

   case TestBeginIndex:
      if(!GetString(text, cli)) return -1;
      cli.EndOfInput(false);
      *cli.obuf << spaces(2) << SuccessExpl << CRLF;
      ConcludeTest(cli);
      InitiateTest(cli, text);
      return 0;

   case TestEndIndex:
      cli.EndOfInput(false);
      *cli.obuf << spaces(2) << SuccessExpl << CRLF;
      ConcludeTest(cli);
      return 0;

   case TestFailedIndex:
      if(!GetIntParm(rc, cli)) return -1;
      if(!GetString(text, cli)) text.clear();
      cli.EndOfInput(false);
      return test->SetFailed(rc, text);

   case TestQueryIndex:
      cli.EndOfInput(false);
      *cli.obuf << spaces(2) << "Passed: " << test->PassCount() << CRLF;
      *cli.obuf << spaces(2) << "Failed: " << test->FailCount() << CRLF;
      return 0;

   case TestResetIndex:
      cli.EndOfInput(false);
      test->ResetPassCount();
      test->ResetFailCount();
      break;

   default:
      return CliCommand::ProcessSubcommand(cli, index);
   }

   return cli.Report(0, SuccessExpl);
}

//==============================================================================
//
//  Testing for LeakyBucketCounter.
//
class LbcPool : public Temporary
{
   friend class Singleton< LbcPool >;
public:
   LeakyBucketCounter lbc_;
private:
   LbcPool() { }
   ~LbcPool() { }
};

class LbcLimitParm : public CliIntParm
{
public: LbcLimitParm();
};

class LbcTimeParm : public CliIntParm
{
public: LbcTimeParm();
};

class LeakyBucketCounterCommands : public CliCommandSet
{
public:
   LeakyBucketCounterCommands();
};

class LbcInitCommand : public CliCommand
{
public:
   LbcInitCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class LbcEventCommand : public CliCommand
{
public:
   LbcEventCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

fixed_string LbcLimitExpl = "capacity of bucket (limit)";

LbcLimitParm::LbcLimitParm() : CliIntParm(LbcLimitExpl, 1, 3600) { }

//------------------------------------------------------------------------------

fixed_string LbcTimeExpl = "time to empty bucket (seconds)";

LbcTimeParm::LbcTimeParm() : CliIntParm(LbcTimeExpl, 1, 3600) { }

//------------------------------------------------------------------------------

fixed_string LeakyBucketCounterStr = "lbc";
fixed_string LeakyBucketCounterExpl = "Tests a LeakyBucketCounter function.";

LeakyBucketCounterCommands::LeakyBucketCounterCommands() :
   CliCommandSet(LeakyBucketCounterStr, LeakyBucketCounterExpl)
{
   BindCommand(*new LbcInitCommand);
   BindCommand(*new LbcEventCommand);
}

//------------------------------------------------------------------------------

fixed_string LbcInitStr = "init";
fixed_string LbcInitExpl = "Initializes the counter.";

LbcInitCommand::LbcInitCommand() : CliCommand(LbcInitStr, LbcInitExpl)
{
   BindParm(*new LbcLimitParm);
   BindParm(*new LbcTimeParm);
}

fn_name LbcInitCommand_ProcessCommand = "LbcInitCommand.ProcessCommand";

word LbcInitCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(LbcInitCommand_ProcessCommand);

   word limit, secs;

   if(!GetIntParm(limit, cli)) return -1;
   if(!GetIntParm(secs, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< LbcPool >::Instance();
   pool->lbc_.Initialize(limit, secs);
   pool->lbc_.Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string LbcEventStr = "event";
fixed_string LbcEventExpl = "Updates the counter when an event occurs.";

LbcEventCommand::LbcEventCommand() : CliCommand(LbcEventStr, LbcEventExpl) { }

fn_name LbcEventCommand_ProcessCommand = "LbcEventCommand.ProcessCommand";

word LbcEventCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(LbcEventCommand_ProcessCommand);

   cli.EndOfInput(false);
   *cli.obuf << spaces(2);
   auto pool = Singleton< LbcPool >::Instance();
   if(pool->lbc_.HasReachedLimit())
      *cli.obuf << "The counter overflowed.";
   else
      *cli.obuf << "The counter did not overflow.";
   *cli.obuf << CRLF;
   pool->lbc_.Output(*cli.obuf, 2, true);
   return 0;
}

//==============================================================================
//
//  Testing for Q1Way.
//
fixed_string NullPtrExpl = "id=nullptr";

class Q1WayItem : public Temporary
{
public:
   explicit Q1WayItem(word index);
   ~Q1WayItem();
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;
   static ptrdiff_t LinkDiff();

   const id_t index_;
private:
   Q1WayItem(const Q1WayItem& that);
   void operator=(const Q1WayItem& that);
   Q1Link link_;
};

class Q1WayPool : public Temporary
{
   friend class Singleton< Q1WayPool >;
public:
   static const size_t MaxItems = 8;
   void Reallocate();
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   std::unique_ptr< Q1WayItem > items_[MaxItems + 1];
   Q1Way< Q1WayItem > itemq_;
private:
   Q1WayPool();
};

class Q1WayItemIndexParm : public CliIntParm
{
public: Q1WayItemIndexParm();
};

class Q1WayCommands : public CliCommandSet
{
public:
   Q1WayCommands();
};

class Countq1Command : public CliCommand
{
public:
   Countq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Deq1Command : public CliCommand
{
public:
   Deq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Emptyq1Command : public CliCommand
{
public:
   Emptyq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Enq1Command : public CliCommand
{
public:
   Enq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Exq1Command : public CliCommand
{
public:
   Exq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Firstq1Command : public CliCommand
{
public:
   Firstq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Henq1Command : public CliCommand
{
public:
   Henq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Insertq1Command : public CliCommand
{
public:
   Insertq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Nextq1Command : public CliCommand
{
public:
   Nextq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Purgeq1Command : public CliCommand
{
public:
   Purgeq1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

Q1WayItem::Q1WayItem(word index) : index_(index) { }

//------------------------------------------------------------------------------

Q1WayItem::~Q1WayItem()
{
   Singleton< Q1WayPool >::Instance()->items_[index_].release();
}

//------------------------------------------------------------------------------

void Q1WayItem::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   if(options.test(DispVerbose))
      stream << prefix << "index=" << index_ << CRLF;
   else
      stream << index_;
}

//------------------------------------------------------------------------------

ptrdiff_t Q1WayItem::LinkDiff()
{
   int local;
   auto fake = reinterpret_cast< const Q1WayItem* >(&local);
   return ptrdiff(&fake->link_, fake);
}

//------------------------------------------------------------------------------

fixed_string Q1WayItemIndexExpl = "item number (0 = nullptr)";

Q1WayItemIndexParm::Q1WayItemIndexParm() :
   CliIntParm(Q1WayItemIndexExpl, 0, Q1WayPool::MaxItems) { }

//------------------------------------------------------------------------------

Q1WayPool::Q1WayPool()
{
   itemq_.Init(Q1WayItem::LinkDiff());

   items_[0] = nullptr;

   for(auto i = 1; i <= MaxItems; ++i)
   {
      items_[i].reset(new Q1WayItem(i));
   }
}

//------------------------------------------------------------------------------

void Q1WayPool::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   stream << prefix << "Q1Way (size=" << itemq_.Size() << "): ";

   for(auto curr = itemq_.First(); curr != nullptr; itemq_.Next(curr))
   {
      curr->Display(stream, prefix + spaces(2), NoFlags);
   }

   stream << CRLF;
}

//------------------------------------------------------------------------------

void Q1WayPool::Reallocate()
{
   for(auto i = 1; i <= MaxItems; ++i)
   {
      if(items_[i] == nullptr)
      {
         items_[i].reset(new Q1WayItem(i));
      }
   }
}

//------------------------------------------------------------------------------

fixed_string Q1WayStr = "q1";
fixed_string Q1WayExpl = "Tests a Q1Way function.";

Q1WayCommands::Q1WayCommands() : CliCommandSet(Q1WayStr, Q1WayExpl)
{
   BindCommand(*new Enq1Command);
   BindCommand(*new Henq1Command);
   BindCommand(*new Insertq1Command);
   BindCommand(*new Deq1Command);
   BindCommand(*new Exq1Command);
   BindCommand(*new Firstq1Command);
   BindCommand(*new Nextq1Command);
   BindCommand(*new Countq1Command);
   BindCommand(*new Emptyq1Command);
   BindCommand(*new Purgeq1Command);
}

//------------------------------------------------------------------------------

fixed_string Countq1Str = "count";
fixed_string Countq1Expl = "Returns the number of items in the queue.";

Countq1Command::Countq1Command() : CliCommand(Countq1Str, Countq1Expl) { }

fn_name Countq1Command_ProcessCommand = "Countq1Command.ProcessCommand";

word Countq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Countq1Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q1WayPool >::Instance();
   *cli.obuf << "  size=" << pool->itemq_.Size() << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Deq1Str = "deq";
fixed_string Deq1Expl = "Removes the item at the front of the queue.";

Deq1Command::Deq1Command() : CliCommand(Deq1Str, Deq1Expl) { }

fn_name Deq1Command_ProcessCommand = "Deq1Command.ProcessCommand";

word Deq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Deq1Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q1WayPool >::Instance();
   auto item = pool->itemq_.Deq();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Emptyq1Str = "empty";
fixed_string Emptyq1Expl = "Returns true if the queue is empty.";

Emptyq1Command::Emptyq1Command() : CliCommand(Emptyq1Str, Emptyq1Expl) { }

fn_name Emptyq1Command_ProcessCommand = "Emptyq1Command.ProcessCommand";

word Emptyq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Emptyq1Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q1WayPool >::Instance();
   auto empty = pool->itemq_.Empty();
   *cli.obuf << "  empty=" << empty << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Enq1Str = "enq";
fixed_string Enq1Expl = "Adds an item to the end of the queue.";

Enq1Command::Enq1Command() : CliCommand(Enq1Str, Enq1Expl)
{
   BindParm(*new Q1WayItemIndexParm);
}

fn_name Enq1Command_ProcessCommand = "Enq1Command.ProcessCommand";

word Enq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Enq1Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q1WayPool >::Instance();
   pool->itemq_.Enq(*pool->items_[id1]);
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Exq1Str = "exq";
fixed_string Exq1Expl = "Removes an item from anywhere in the queue.";

Exq1Command::Exq1Command() : CliCommand(Exq1Str, Exq1Expl)
{
   BindParm(*new Q1WayItemIndexParm);
}

fn_name Exq1Command_ProcessCommand = "Exq1Command.ProcessCommand";

word Exq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Exq1Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q1WayPool >::Instance();
   pool->itemq_.Exq(*pool->items_[id1]);
   pool->items_[id1]->Output(*cli.obuf, 2, true);
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Firstq1Str = "first";
fixed_string Firstq1Expl = "Returns the first item in the queue.";

Firstq1Command::Firstq1Command() : CliCommand(Firstq1Str, Firstq1Expl) { }

fn_name Firstq1Command_ProcessCommand = "Firstq1Command.ProcessCommand";

word Firstq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Firstq1Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q1WayPool >::Instance();

   auto item = pool->itemq_.First();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Henq1Str = "henq";
fixed_string Henq1Expl = "Adds an item to the front of the queue.";

Henq1Command::Henq1Command() : CliCommand(Henq1Str, Henq1Expl)
{
   BindParm(*new Q1WayItemIndexParm);
}

fn_name Henq1Command_ProcessCommand = "Henq1Command.ProcessCommand";

word Henq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Henq1Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q1WayPool >::Instance();
   pool->itemq_.Henq(*pool->items_[id1]);
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Insertq1Str = "insert";
fixed_string Insertq1Expl = "Inserts item#2 after item#1.";

Insertq1Command::Insertq1Command() : CliCommand(Insertq1Str, Insertq1Expl)
{
   BindParm(*new Q1WayItemIndexParm);
   BindParm(*new Q1WayItemIndexParm);
}

fn_name Insertq1Command_ProcessCommand = "Insertq1Command.ProcessCommand";

word Insertq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Insertq1Command_ProcessCommand);

   word id1, id2;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(id2, cli)) return -1;
   cli.EndOfInput(false);
   if(id2 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q1WayPool >::Instance();
   pool->itemq_.Insert(pool->items_[id1].get(), *pool->items_[id2]);
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Nextq1Str = "next";
fixed_string Nextq1Expl = "Returns the next item in the queue.";

Nextq1Command::Nextq1Command() : CliCommand(Nextq1Str, Nextq1Expl)
{
   BindParm(*new Q1WayItemIndexParm);
}

fn_name Nextq1Command_ProcessCommand = "Nextq1Command.ProcessCommand";

word Nextq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Nextq1Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< Q1WayPool >::Instance();
   auto item = pool->items_[id1].get();

   *cli.obuf << "Next(T*&): " << CRLF;
   pool->itemq_.Next(item);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   *cli.obuf << "T*=Next(T&): " << CRLF;
   if(id1 != 0)
   {
      item = pool->itemq_.Next(*pool->items_[id1]);
      if(item != nullptr)
         item->Output(*cli.obuf, 2, true);
      else
         *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   }
   else
   {
      *cli.obuf << NullPtrInvalid << CRLF;
   }

   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Purgeq1Str = "purge";
fixed_string Purgeq1Expl = "Deletes all the items in the queue.";

Purgeq1Command::Purgeq1Command() : CliCommand(Purgeq1Str, Purgeq1Expl) { }

fn_name Purgeq1Command_ProcessCommand = "Purgeq1Command.ProcessCommand";

word Purgeq1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Purgeq1Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q1WayPool >::Instance();
   pool->itemq_.Purge();
   pool->Output(*cli.obuf, 2, false);
   pool->Reallocate();
   return 0;
}

//==============================================================================
//
//  Testing for Q2Way.
//
class Q2WayItem : public Temporary
{
public:
   explicit Q2WayItem(word index);
   ~Q2WayItem();
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;
   static ptrdiff_t LinkDiff();

   const id_t index_;
private:
   Q2WayItem(const Q2WayItem& that);
   void operator=(const Q2WayItem& that);
   Q2Link link_;
};

class Q2WayPool : public Temporary
{
   friend class Singleton< Q2WayPool >;
public:
   static const size_t MaxItems = 8;
   void Reallocate();
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   std::unique_ptr< Q2WayItem > items_[MaxItems + 1];
   Q2Way< Q2WayItem > itemq_;
private:
   Q2WayPool();
};

class Q2WayItemIndexParm : public CliIntParm
{
public: Q2WayItemIndexParm();
};

class Q2WayCommands : public CliCommandSet
{
public:
   Q2WayCommands();
};

class Countq2Command : public CliCommand
{
public:
   Countq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Deq2Command : public CliCommand
{
public:
   Deq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Emptyq2Command : public CliCommand
{
public:
   Emptyq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Enq2Command : public CliCommand
{
public:
   Enq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Exq2Command : public CliCommand
{
public:
   Exq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Firstq2Command : public CliCommand
{
public:
   Firstq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Henq2Command : public CliCommand
{
public:
   Henq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Lastq2Command : public CliCommand
{
public:
   Lastq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Nextq2Command : public CliCommand
{
public:
   Nextq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Prevq2Command : public CliCommand
{
public:
   Prevq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class Purgeq2Command : public CliCommand
{
public:
   Purgeq2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

Q2WayItem::Q2WayItem(word index) : index_(index) { }

//------------------------------------------------------------------------------

Q2WayItem::~Q2WayItem()
{
   Singleton< Q2WayPool >::Instance()->items_[index_].release();
}

//------------------------------------------------------------------------------

void Q2WayItem::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   if(options.test(DispVerbose))
      stream << prefix << "index=" << index_ << CRLF;
   else
      stream << index_;
}

//------------------------------------------------------------------------------

ptrdiff_t Q2WayItem::LinkDiff()
{
   int local;
   auto fake = reinterpret_cast< const Q2WayItem* >(&local);
   return ptrdiff(&fake->link_, fake);
}

//------------------------------------------------------------------------------

fixed_string Q2WayItemIndexExpl = "item number (0 = nullptr)";

Q2WayItemIndexParm::Q2WayItemIndexParm() :
   CliIntParm(Q2WayItemIndexExpl, 0, Q2WayPool::MaxItems) { }

//------------------------------------------------------------------------------

Q2WayPool::Q2WayPool()
{
   itemq_.Init(Q2WayItem::LinkDiff());

   items_[0] = nullptr;

   for(auto i = 1; i <= MaxItems; ++i)
   {
      items_[i].reset(new Q2WayItem(i));
   }
}

//------------------------------------------------------------------------------

void Q2WayPool::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   stream << prefix << "Q2Way (size=" << itemq_.Size() << "): ";

   for(auto curr = itemq_.First(); curr != nullptr; itemq_.Next(curr))
   {
      curr->Display(stream, prefix + spaces(2), NoFlags);
   }

   stream << CRLF;
}

//------------------------------------------------------------------------------

void Q2WayPool::Reallocate()
{
   for(auto i = 1; i <= MaxItems; ++i)
   {
      if(items_[i] == nullptr)
      {
         items_[i].reset(new Q2WayItem(i));
      }
   }
}

//------------------------------------------------------------------------------

fixed_string Q2WayStr = "q2";
fixed_string Q2WayExpl = "Tests a Q2Way function.";

Q2WayCommands::Q2WayCommands() : CliCommandSet(Q2WayStr, Q2WayExpl)
{
   BindCommand(*new Enq2Command);
   BindCommand(*new Henq2Command);
   BindCommand(*new Deq2Command);
   BindCommand(*new Exq2Command);
   BindCommand(*new Firstq2Command);
   BindCommand(*new Nextq2Command);
   BindCommand(*new Lastq2Command);
   BindCommand(*new Prevq2Command);
   BindCommand(*new Countq2Command);
   BindCommand(*new Emptyq2Command);
   BindCommand(*new Purgeq2Command);
}

//------------------------------------------------------------------------------

fixed_string Countq2Str = "count";
fixed_string Countq2Expl = "Returns the number of items in the queue.";

Countq2Command::Countq2Command() : CliCommand(Countq2Str, Countq2Expl) { }

fn_name Countq2Command_ProcessCommand = "Countq2Command.ProcessCommand";

word Countq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Countq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();
   *cli.obuf << "  size=" << pool->itemq_.Size() << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Deq2Str = "deq";
fixed_string Deq2Expl = "Removes the item at the front of the queue.";

Deq2Command::Deq2Command() : CliCommand(Deq2Str, Deq2Expl) { }

fn_name Deq2Command_ProcessCommand = "Deq2Command.ProcessCommand";

word Deq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Deq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();
   auto item = pool->itemq_.Deq();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Emptyq2Str = "empty";
fixed_string Emptyq2Expl = "Returns true if the queue is empty.";

Emptyq2Command::Emptyq2Command() : CliCommand(Emptyq2Str, Emptyq2Expl) { }

fn_name Emptyq2Command_ProcessCommand = "Emptyq2Command.ProcessCommand";

word Emptyq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Emptyq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();
   auto empty = pool->itemq_.Empty();
   *cli.obuf << "  empty=" << empty << CRLF;
   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Enq2Str = "enq";
fixed_string Enq2Expl = "Adds an item to the end of the queue.";

Enq2Command::Enq2Command() : CliCommand(Enq2Str, Enq2Expl)
{
   BindParm(*new Q2WayItemIndexParm);
}

fn_name Enq2Command_ProcessCommand = "Enq2Command.ProcessCommand";

word Enq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Enq2Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q2WayPool >::Instance();
   pool->itemq_.Enq(*pool->items_[id1]);
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Exq2Str = "exq";
fixed_string Exq2Expl = "Removes an item from anywhere in the queue.";

Exq2Command::Exq2Command() : CliCommand(Exq2Str, Exq2Expl)
{
   BindParm(*new Q2WayItemIndexParm);
}

fn_name Exq2Command_ProcessCommand = "Exq2Command.ProcessCommand";

word Exq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Exq2Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q2WayPool >::Instance();
   pool->itemq_.Exq(*pool->items_[id1]);
   pool->items_[id1]->Output(*cli.obuf, 2, true);
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Firstq2Str = "first";
fixed_string Firstq2Expl = "Returns the first item in the queue.";

Firstq2Command::Firstq2Command() : CliCommand(Firstq2Str, Firstq2Expl) { }

fn_name Firstq2Command_ProcessCommand = "Firstq2Command.ProcessCommand";

word Firstq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Firstq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();

   *cli.obuf << "T*=First(): " << CRLF;
   auto item = pool->itemq_.First();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Henq2Str = "henq";
fixed_string Henq2Expl = "Adds an item to the front of the queue.";

Henq2Command::Henq2Command() : CliCommand(Henq2Str, Henq2Expl)
{
   BindParm(*new Q2WayItemIndexParm);
}

fn_name Henq2Command_ProcessCommand = "Henq2Command.ProcessCommand";

word Henq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Henq2Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   if(id1 == 0)
   {
      *cli.obuf << NullPtrInvalid << CRLF;
      return -1;
   }
   auto pool = Singleton< Q2WayPool >::Instance();
   pool->itemq_.Henq(*pool->items_[id1]);
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Lastq2Str = "last";
fixed_string Lastq2Expl = "Returns the last item in the queue.";

Lastq2Command::Lastq2Command() : CliCommand(Lastq2Str, Lastq2Expl) { }

fn_name Lastq2Command_ProcessCommand = "Lastq2Command.ProcessCommand";

word Lastq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Lastq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();

   *cli.obuf << "T*=Last(): " << CRLF;
   auto item = pool->itemq_.Last();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Nextq2Str = "next";
fixed_string Nextq2Expl = "Returns the next item in the queue.";

Nextq2Command::Nextq2Command() : CliCommand(Nextq2Str, Nextq2Expl)
{
   BindParm(*new Q2WayItemIndexParm);
}

fn_name Nextq2Command_ProcessCommand = "Nextq2Command.ProcessCommand";

word Nextq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Nextq2Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< Q2WayPool >::Instance();
   auto item = pool->items_[id1].get();

   *cli.obuf << "Next(T*&): " << CRLF;
   pool->itemq_.Next(item);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   *cli.obuf << "T*=Next(T&): " << CRLF;
   if(id1 != 0)
   {
      item = pool->itemq_.Next(*pool->items_[id1]);
      if(item != nullptr)
         item->Output(*cli.obuf, 2, true);
      else
         *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   }
   else
   {
      *cli.obuf << NullPtrInvalid << CRLF;
   }

   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Prevq2Str = "prev";
fixed_string Prevq2Expl = "Returns the previous item.";

Prevq2Command::Prevq2Command() : CliCommand(Prevq2Str, Prevq2Expl)
{
   BindParm(*new Q2WayItemIndexParm);
}

fn_name Prevq2Command_ProcessCommand = "Prevq2Command.ProcessCommand";

word Prevq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Prevq2Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< Q2WayPool >::Instance();
   auto item = pool->items_[id1].get();

   *cli.obuf << "Prev(T*&): " << CRLF;
   pool->itemq_.Prev(item);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   *cli.obuf << "T*=Prev(T&): " << CRLF;
   if(id1 != 0)
   {
      item = pool->itemq_.Prev(*pool->items_[id1]);
      if(item != nullptr)
         item->Output(*cli.obuf, 2, true);
      else
         *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   }
   else
   {
      *cli.obuf << NullPtrInvalid << CRLF;
   }

   pool->Output(*cli.obuf, 2, false);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string Purgeq2Str = "purge";
fixed_string Purgeq2Expl = "Deletes all the items in the queue.";

Purgeq2Command::Purgeq2Command() : CliCommand(Purgeq2Str, Purgeq2Expl) { }

fn_name Purgeq2Command_ProcessCommand = "Purgeq2Command.ProcessCommand";

word Purgeq2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(Purgeq2Command_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< Q2WayPool >::Instance();
   pool->itemq_.Purge();
   pool->Output(*cli.obuf, 2, false);
   pool->Reallocate();
   return 0;
}

//==============================================================================
//
//  Testing for Registry.
//
class RegistryItem : public Temporary
{
public:
   explicit RegistryItem(word index);
   ~RegistryItem();
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;
   static ptrdiff_t CellDiff();

   RegCell rid_;
private:
   RegistryItem(const RegistryItem& that);
   void operator=(const RegistryItem& that);
   const id_t index_;
};

class RegistryPool : public Temporary
{
   friend class Singleton< RegistryPool >;
public:
   static const size_t MaxItems = 8;
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;

   std::unique_ptr< RegistryItem > items_[MaxItems + 1];
   Registry< RegistryItem > registry_;
private:
   RegistryPool();
};

class RegistryItemIndexParm : public CliIntParm
{
public: RegistryItemIndexParm();
};

class RegistryIdMandParm : public CliIntParm
{
public: RegistryIdMandParm();
};

class RegistryIdOptParm : public CliIntParm
{
public: RegistryIdOptParm();
};

class RegistrySizeParm : public CliIntParm
{
public: RegistrySizeParm();
};

class RegistryCommands : public CliCommandSet
{
public:
   RegistryCommands();
};

class InitCommand : public CliCommand
{
public:
   InitCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class InsertCommand : public CliCommand
{
public:
   InsertCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class RemoveCommand : public CliCommand
{
public:
   RemoveCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class AtCommand : public CliCommand
{
public:
   AtCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class FirstCommand : public CliCommand
{
public:
   FirstCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class NextCommand : public CliCommand
{
public:
   NextCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class LastCommand : public CliCommand
{
public:
   LastCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class PrevCommand : public CliCommand
{
public:
   PrevCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class CountCommand : public CliCommand
{
public:
   CountCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

RegistryItem::RegistryItem(word index) : index_(index) { }

//------------------------------------------------------------------------------

RegistryItem::~RegistryItem()
{
   Singleton< RegistryPool >::Instance()->items_[index_].release();
}

//------------------------------------------------------------------------------

void RegistryItem::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   stream << prefix << "index=" << index_ << CRLF;
}

//------------------------------------------------------------------------------

ptrdiff_t RegistryItem::CellDiff()
{
   int local;
   auto fake = reinterpret_cast< const RegistryItem* >(&local);
   return ptrdiff(&fake->rid_, fake);
}

//------------------------------------------------------------------------------

fixed_string RegistryItemIndexExpl = "item number (0 = nullptr)";

RegistryItemIndexParm::RegistryItemIndexParm() :
   CliIntParm(RegistryItemIndexExpl, 0, RegistryPool::MaxItems) { }

//------------------------------------------------------------------------------

RegistryPool::RegistryPool()
{
   items_[0] = nullptr;

   for(auto i = 1; i <= MaxItems; ++i)
   {
      items_[i].reset(new RegistryItem(i));
   }
}

//------------------------------------------------------------------------------

void RegistryPool::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   stream << prefix << "Registry:" << CRLF;
   registry_.Display(stream, prefix + spaces(2), Flags(Vb_Mask));
   stream << CRLF;
}

//------------------------------------------------------------------------------

fixed_string RegistryIdExpl = "registrant id";

RegistryIdMandParm::RegistryIdMandParm() :
   CliIntParm(RegistryIdExpl, 0, 31) { }

RegistryIdOptParm::RegistryIdOptParm() :
   CliIntParm(RegistryIdExpl, 0, 31, true) { }

//------------------------------------------------------------------------------

fixed_string RegistrySizeExpl = "maximum number of items in registry";

RegistrySizeParm::RegistrySizeParm() :
   CliIntParm(RegistrySizeExpl, 0, RegistryPool::MaxItems) { }

//------------------------------------------------------------------------------

fixed_string RegistryStr = "reg";
fixed_string RegistryExpl = "Tests a Registry function.";

RegistryCommands::RegistryCommands() :
   CliCommandSet(RegistryStr, RegistryExpl)
{
   BindCommand(*new InitCommand);
   BindCommand(*new InsertCommand);
   BindCommand(*new RemoveCommand);
   BindCommand(*new AtCommand);
   BindCommand(*new FirstCommand);
   BindCommand(*new NextCommand);
   BindCommand(*new LastCommand);
   BindCommand(*new PrevCommand);
   BindCommand(*new CountCommand);
}

//------------------------------------------------------------------------------

fixed_string InitStr = "init";
fixed_string InitExpl = "Initializes the registry.";

InitCommand::InitCommand() : CliCommand(InitStr, InitExpl)
{
   BindParm(*new RegistrySizeParm);
}

fn_name InitCommand_ProcessCommand = "InitCommand.ProcessCommand";

word InitCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(InitCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   auto result = pool->registry_.Init
      (id1, RegistryItem::CellDiff(), MemTemp, false);
   *cli.obuf << "  rc=" << result << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string InsertStr = "insert";
fixed_string InsertExpl = "Adds an item to the registry.";

InsertCommand::InsertCommand() : CliCommand(InsertStr, InsertExpl)
{
   BindParm(*new RegistryItemIndexParm);
   BindParm(*new RegistryIdOptParm);
}

fn_name InsertCommand_ProcessCommand = "InsertCommand.ProcessCommand";

word InsertCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(InsertCommand_ProcessCommand);

   word id1, id2;
   bool fixed;

   if(!GetIntParm(id1, cli)) return -1;

   switch(GetIntParmRc(id2, cli))
   {
   case None: fixed = false; break;
   case Ok: fixed = true; break;
   default: return -1;
   }

   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   if(id1 > 0)
   {
      if(fixed)
         pool->items_[id1]->rid_.SetId(id2);
      else
         pool->items_[id1]->rid_.SetId(0);
   }
   auto result = pool->registry_.Insert(*pool->items_[id1]);
   *cli.obuf << "  rc=" << result << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string RemoveStr = "remove";
fixed_string RemoveExpl = "Removes an item from the registry.";

RemoveCommand::RemoveCommand() : CliCommand(RemoveStr, RemoveExpl)
{
   BindParm(*new RegistryItemIndexParm);
   BindParm(*new RegistryIdOptParm);
}

fn_name RemoveCommand_ProcessCommand = "RemoveCommand.ProcessCommand";

word RemoveCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(RemoveCommand_ProcessCommand);

   word id1, id2;
   bool fixed, result;

   if(!GetIntParm(id1, cli)) return -1;

   switch(GetIntParmRc(id2, cli))
   {
   case None: fixed = false; break;
   case Ok: fixed = true; break;
   default: return -1;
   }

   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   if(fixed)
      result = pool->registry_.Erase(*pool->items_[id1], id2);
   else
      result = pool->registry_.Erase(*pool->items_[id1]);
   *cli.obuf << "  rc=" << result << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string AtStr = "at";
fixed_string AtExpl = "Accesses an item in the registry.";

AtCommand::AtCommand() : CliCommand(AtStr, AtExpl)
{
   BindParm(*new RegistryIdMandParm);
}

fn_name AtCommand_ProcessCommand = "AtCommand.ProcessCommand";

word AtCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(AtCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   auto item = pool->registry_.At(id1);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string FirstStr = "first";
fixed_string FirstExpl = "Returns the first item in the registry.";

FirstCommand::FirstCommand() : CliCommand(FirstStr, FirstExpl)
{
   BindParm(*new RegistryIdOptParm);
}

fn_name FirstCommand_ProcessCommand = "FirstCommand.ProcessCommand";

word FirstCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(FirstCommand_ProcessCommand);

   word id1;
   bool start;
   id_t rid;
   RegistryItem* item;

   switch(GetIntParmRc(id1, cli))
   {
   case None: start = false; break;
   case Ok: start = true; break;
   default: return -1;
   }

   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   if(start)
   {
      rid = id1;
      item = pool->registry_.First(rid);
   }
   else
   {
      item = pool->registry_.First();
   }
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string NextStr = "next";
fixed_string NextExpl = "Returns the next item in the registry.";

NextCommand::NextCommand() : CliCommand(NextStr, NextExpl)
{
   BindParm(*new RegistryItemIndexParm);
}

fn_name NextCommand_ProcessCommand = "NextCommand.ProcessCommand";

word NextCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(NextCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< RegistryPool >::Instance();
   auto item = pool->items_[id1].get();

   *cli.obuf << "Next(T*&): " << CRLF;
   pool->registry_.Next(item);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   *cli.obuf << "T*=Next(T&): " << CRLF;
   if(id1 != 0)
   {
      item = pool->registry_.Next(*pool->items_[id1]);
      if(item != nullptr)
         item->Output(*cli.obuf, 2, true);
      else
         *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   }
   else
   {
      *cli.obuf << NullPtrInvalid << CRLF;
   }

   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string LastStr = "last";
fixed_string LastExpl = "Returns the last item in the registry.";

LastCommand::LastCommand() : CliCommand(LastStr, LastExpl) { }

fn_name LastCommand_ProcessCommand = "LastCommand.ProcessCommand";

word LastCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(LastCommand_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   auto item = pool->registry_.Last();
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string PrevStr = "prev";
fixed_string PrevExpl = "Returns the previous item in the registry.";

PrevCommand::PrevCommand() : CliCommand(PrevStr, PrevExpl)
{
   BindParm(*new RegistryItemIndexParm);
}

fn_name PrevCommand_ProcessCommand = "PrevCommand.ProcessCommand";

word PrevCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(PrevCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);

   auto pool = Singleton< RegistryPool >::Instance();
   auto item = pool->items_[id1].get();

   *cli.obuf << "Prev(T*&): " << CRLF;
   pool->registry_.Prev(item);
   if(item != nullptr)
      item->Output(*cli.obuf, 2, true);
   else
      *cli.obuf << spaces(2) << NullPtrExpl << CRLF;

   *cli.obuf << "T*=Prev(T&): " << CRLF;
   if(id1 != 0)
   {
      item = pool->registry_.Prev(*pool->items_[id1]);
      if(item != nullptr)
         item->Output(*cli.obuf, 2, true);
      else
         *cli.obuf << spaces(2) << NullPtrExpl << CRLF;
   }
   else
   {
      *cli.obuf << NullPtrInvalid << CRLF;
   }

   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//------------------------------------------------------------------------------

fixed_string CountStr = "count";
fixed_string CountExpl = "Returns the number of items in the registry.";

CountCommand::CountCommand() : CliCommand(CountStr, CountExpl) { }

fn_name CountCommand_ProcessCommand = "CountCommand.ProcessCommand";

word CountCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(CountCommand_ProcessCommand);

   cli.EndOfInput(false);
   auto pool = Singleton< RegistryPool >::Instance();
   *cli.obuf << "  size=" << pool->registry_.Size() << CRLF;
   pool->Output(*cli.obuf, 2, true);
   return 0;
}

//==============================================================================
//
//  Testing for SysTime.
//
class SysTimePool : public Temporary
{
   friend class Singleton< SysTimePool >;
public:
   static const size_t MaxIndex = 3;

   SysTime time_[MaxIndex + 1];
private:
   SysTimePool() { }
   ~SysTimePool() { }
};

class SysTimeIndexParm : public CliIntParm
{
public: SysTimeIndexParm();
};

class SysTimeIntervalParm : public CliIntParm
{
public: SysTimeIntervalParm();
};

class SysTimeMsecsParm : public CliIntParm
{
public: SysTimeMsecsParm();
};

class SysTimeDaysParm : public CliIntParm
{
public: SysTimeDaysParm();
};

class SysTimeCommands : public CliCommandSet
{
public:
   SysTimeCommands();
};

class TimeCtor1Command : public CliCommand
{
public:
   TimeCtor1Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class TimeCtor2Command : public CliCommand
{
public:
   TimeCtor2Command();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class DayOfWeekCommand : public CliCommand
{
public:
   DayOfWeekCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class DayOfYearCommand : public CliCommand
{
public:
   DayOfYearCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class IsLeapYearCommand : public CliCommand
{
public:
   IsLeapYearCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class TruncateCommand : public CliCommand
{
public:
   TruncateCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class RoundCommand : public CliCommand
{
public:
   RoundCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class AddMsecsCommand : public CliCommand
{
public:
   AddMsecsCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class SubMsecsCommand : public CliCommand
{
public:
   SubMsecsCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class MsecsFromNowCommand : public CliCommand
{
public:
   MsecsFromNowCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class MsecsUntilCommand : public CliCommand
{
public:
   MsecsUntilCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class AddDaysCommand : public CliCommand
{
public:
   AddDaysCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class SubDaysCommand : public CliCommand
{
public:
   SubDaysCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

class StrTimeCommand : public CliCommand
{
public:
   StrTimeCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

fixed_string SysTimeIndexExpl = "item number";

SysTimeIndexParm::SysTimeIndexParm() :
   CliIntParm(SysTimeIndexExpl, 1, SysTimePool::MaxIndex) { }

//------------------------------------------------------------------------------

fixed_string SysTimeIntervalExpl =
   "interval (must evenly divide the field's range)";

SysTimeIntervalParm::SysTimeIntervalParm() :
   CliIntParm(SysTimeIntervalExpl, 1, 500) { }

//------------------------------------------------------------------------------

fixed_string SysTimeMsecsExpl = "number of milliseconds";

SysTimeMsecsParm::SysTimeMsecsParm() :
   CliIntParm(SysTimeMsecsExpl, WORD_MIN, WORD_MAX) { }

//------------------------------------------------------------------------------

fixed_string SysTimeDaysExpl = "number of days";

SysTimeDaysParm::SysTimeDaysParm() :
   CliIntParm(SysTimeDaysExpl, WORD_MIN, WORD_MAX) { }

//------------------------------------------------------------------------------

fixed_string SysTimeStr = "time";
fixed_string SysTimeExpl = "Tests a SysTime function.";

SysTimeCommands::SysTimeCommands() : CliCommandSet(SysTimeStr, SysTimeExpl)
{
   BindCommand(*new TimeCtor1Command);
   BindCommand(*new TimeCtor2Command);
   BindCommand(*new DayOfWeekCommand);
   BindCommand(*new DayOfYearCommand);
   BindCommand(*new IsLeapYearCommand);
   BindCommand(*new TruncateCommand);
   BindCommand(*new RoundCommand);
   BindCommand(*new AddMsecsCommand);
   BindCommand(*new SubMsecsCommand);
   BindCommand(*new MsecsFromNowCommand);
   BindCommand(*new MsecsUntilCommand);
   BindCommand(*new AddDaysCommand);
   BindCommand(*new SubDaysCommand);
   BindCommand(*new StrTimeCommand);
}

//------------------------------------------------------------------------------

fixed_string TimeCtor1Str = "ctor1";
fixed_string TimeCtor1Expl = "Constructs the current time.";

TimeCtor1Command::TimeCtor1Command() : CliCommand(TimeCtor1Str, TimeCtor1Expl)
{
   BindParm(*new SysTimeIndexParm);
}

fn_name TimeCtor1Command_ProcessCommand = "TimeCtor1Command.ProcessCommand";

word TimeCtor1Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TimeCtor1Command_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1] = SysTime();
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string TimeCtor2Str = "ctor2";
fixed_string TimeCtor2Expl = "Constructs a specified time.";

TimeCtor2Command::TimeCtor2Command() : CliCommand(TimeCtor2Str, TimeCtor2Expl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeYearParm);
   BindParm(*new SysTimeMonthParm);
   BindParm(*new SysTimeDayParm);
   BindParm(*new SysTimeHourParm);
   BindParm(*new SysTimeMinuteParm);
   BindParm(*new SysTimeSecondParm);
   BindParm(*new SysTimeMsecondParm);
}

fn_name TimeCtor2Command_ProcessCommand = "TimeCtor2Command.ProcessCommand";

word TimeCtor2Command::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TimeCtor2Command_ProcessCommand);

   word id1, year, month, day, hour, min, sec, msec;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(year, cli)) return -1;
   if(!GetIntParm(month, cli)) return -1;
   if(!GetIntParm(day, cli)) return -1;
   if(!GetIntParm(hour, cli)) return -1;
   if(!GetIntParm(min, cli)) return -1;
   if(!GetIntParm(sec, cli)) return -1;
   if(!GetIntParm(msec, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1] = SysTime(year, month - 1, day, hour, min, sec, msec);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string DayOfWeekStr = "dayofweek";
fixed_string DayOfWeekExpl = "Returns the time's day of the week.";

DayOfWeekCommand::DayOfWeekCommand() : CliCommand(DayOfWeekStr, DayOfWeekExpl)
{
   BindParm(*new SysTimeIndexParm);
}

fn_name DayOfWeekCommand_ProcessCommand = "DayOfWeekCommand.ProcessCommand";

word DayOfWeekCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(DayOfWeekCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   *cli.obuf << "  day=" << pool->time_[id1].strWeekDay() << CRLF;
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string DayOfYearStr = "dayofyear";
fixed_string DayOfYearExpl = "Returns the time's day of the year.";

DayOfYearCommand::DayOfYearCommand() : CliCommand(DayOfYearStr, DayOfYearExpl)
{
   BindParm(*new SysTimeIndexParm);
}

fn_name DayOfYearCommand_ProcessCommand = "DayOfYearCommand.ProcessCommand";

word DayOfYearCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(DayOfYearCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   *cli.obuf << "  day=" << pool->time_[id1].DayOfYear() + 1 << CRLF;
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string IsLeapYearStr = "isleapyear";
fixed_string IsLeapYearExpl = "Returns true if a year is a leap year.";

IsLeapYearCommand::IsLeapYearCommand() :
   CliCommand(IsLeapYearStr, IsLeapYearExpl)
{
   BindParm(*new SysTimeYearParm);
}

fn_name IsLeapYearCommand_ProcessCommand = "IsLeapYearCommand.ProcessCommand";

word IsLeapYearCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(IsLeapYearCommand_ProcessCommand);

   word year;

   if(!GetIntParm(year, cli)) return -1;
   cli.EndOfInput(false);
   *cli.obuf << "  leap year=" << SysTime::IsLeapYear(year) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string TruncateStr = "truncate";
fixed_string TruncateExpl = "Truncates the time at a specified field.";

TruncateCommand::TruncateCommand() : CliCommand(TruncateStr, TruncateExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeFieldParm);
}

fn_name TruncateCommand_ProcessCommand = "TruncateCommand.ProcessCommand";

word TruncateCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TruncateCommand_ProcessCommand);

   id_t field;
   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetTextIndex(field, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].Truncate(TimeField(field - 1));
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string RoundStr = "round";
fixed_string RoundExpl = "Rounds off the time at a specified field.";

RoundCommand::RoundCommand() : CliCommand(RoundStr, RoundExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeFieldParm);
   BindParm(*new SysTimeIntervalParm);
}

fn_name RoundCommand_ProcessCommand = "RoundCommand.ProcessCommand";

word RoundCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(RoundCommand_ProcessCommand);

   word id1, interval;
   id_t field;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetTextIndex(field, cli)) return -1;
   if(!GetIntParm(interval, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].Round(TimeField(field - 1), interval);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string AddMsecsStr = "addmsecs";
fixed_string AddMsecsExpl = "Adds milliseconds to the time.";

AddMsecsCommand::AddMsecsCommand() : CliCommand(AddMsecsStr, AddMsecsExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeMsecsParm);
}

fn_name AddMsecsCommand_ProcessCommand = "AddMsecsCommand.ProcessCommand";

word AddMsecsCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(AddMsecsCommand_ProcessCommand);

   word id1, msecs;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(msecs, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].AddMsecs(msecs);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string SubMsecsStr = "submsecs";
fixed_string SubMsecsExpl = "Subtracts milliseconds from the time.";

SubMsecsCommand::SubMsecsCommand() : CliCommand(SubMsecsStr, SubMsecsExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeMsecsParm);
}

fn_name SubMsecsCommand_ProcessCommand = "SubMsecsCommand.ProcessCommand";

word SubMsecsCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(SubMsecsCommand_ProcessCommand);

   word id1, msecs;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(msecs, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].SubMsecs(msecs);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string MsecsFromNowStr = "msecsfromnow";
fixed_string MsecsFromNowExpl = "Returns the milliseconds from now to a time.";

MsecsFromNowCommand::MsecsFromNowCommand() :
   CliCommand(MsecsFromNowStr, MsecsFromNowExpl)
{
   BindParm(*new SysTimeIndexParm);
}

fn_name MsecsFromNowCommand_ProcessCommand =
   "MsecsFromNowCommand.ProcessCommand";

word MsecsFromNowCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(MsecsFromNowCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   *cli.obuf << "  msecs=" << pool->time_[id1].MsecsFromNow() << CRLF;
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string MsecsUntilStr = "msecsuntil";
fixed_string MsecsUntilExpl =
   "Returns the milliseconds from one time to another.";

MsecsUntilCommand::MsecsUntilCommand() :
   CliCommand(MsecsUntilStr, MsecsUntilExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeIndexParm);
}

fn_name MsecsUntilCommand_ProcessCommand = "MsecsUntilCommand.ProcessCommand";

word MsecsUntilCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(MsecsUntilCommand_ProcessCommand);

   word id1, id2;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(id2, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   *cli.obuf << "  msecs=" << pool->time_[id1].MsecsUntil(pool->time_[id2])
      << CRLF;
   *cli.obuf << "  time1=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   *cli.obuf << "  time2=" << pool->time_[id2].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string AddDaysStr = "adddays";
fixed_string AddDaysExpl = "Adds days to the time.";

AddDaysCommand::AddDaysCommand() : CliCommand(AddDaysStr, AddDaysExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeDaysParm);
}

fn_name AddDaysCommand_ProcessCommand = "AddDaysCommand.ProcessCommand";

word AddDaysCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(AddDaysCommand_ProcessCommand);

   word id1, days;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(days, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].AddDays(days);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string SubDaysStr = "subdays";
fixed_string SubDaysExpl = "Subtracts days from the time.";

SubDaysCommand::SubDaysCommand() : CliCommand(SubDaysStr, SubDaysExpl)
{
   BindParm(*new SysTimeIndexParm);
   BindParm(*new SysTimeDaysParm);
}

fn_name SubDaysCommand_ProcessCommand = "SubDaysCommand.ProcessCommand";

word SubDaysCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(SubDaysCommand_ProcessCommand);

   word id1, days;

   if(!GetIntParm(id1, cli)) return -1;
   if(!GetIntParm(days, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   pool->time_[id1].SubDays(days);
   *cli.obuf << "  time=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   return 0;
}

//------------------------------------------------------------------------------

fixed_string StrTimeStr = "strtime";
fixed_string StrTimeExpl = "Displays the time in various formats.";

StrTimeCommand::StrTimeCommand() : CliCommand(StrTimeStr, StrTimeExpl)
{
   BindParm(*new SysTimeIndexParm);
}

fn_name StrTimeCommand_ProcessCommand = "StrTimeCommand.ProcessCommand";

word StrTimeCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(StrTimeCommand_ProcessCommand);

   word id1;

   if(!GetIntParm(id1, cli)) return -1;
   cli.EndOfInput(false);
   auto pool = Singleton< SysTimePool >::Instance();
   *cli.obuf << "   a=" << pool->time_[id1].to_str(SysTime::Alpha) << CRLF;
   *cli.obuf << "  la=" << pool->time_[id1].to_str(SysTime::LowAlpha) << CRLF;
   *cli.obuf << "   n=" << pool->time_[id1].to_str(SysTime::Numeric) << CRLF;
   *cli.obuf << "  hn=" << pool->time_[id1].to_str(SysTime::HighNumeric)
      << CRLF;
   return 0;
}

//==============================================================================
//
//  Thread for testing the Thread safety net.
//
class RecoveryTestThread : public Thread
{
   friend class Singleton< RecoveryTestThread >;
public:
   typedef id_t Test;

   static const Test Sleep           = 0;
   static const Test Abort           = 1;
   static const Test Delete          = 2;
   static const Test DerefenceBadPtr = 3;
   static const Test DivideByZero    = 4;
   static const Test InfiniteLoop    = 5;
   static const Test OverflowStack   = 6;
   static const Test RaiseSignal     = 7;
   static const Test Return          = 8;
   static const Test Swerr           = 9;
   static const Test Terminate       = 10;
   static const Test Trap            = 11;

   void SetTest(Test test) { test_ = test; }
   void SetTestSignal(signal_t signal) {signal_ = signal; }
   virtual void Display(std::ostream& stream,
      const std::string& prefix, const Flags& options) const override;
private:
   RecoveryTestThread();
   ~RecoveryTestThread();
   static void UseBadPointer();
   static void LoopForever();
   static void RecurseForever(size_t depth);
   virtual const char* AbbrName() const override;
   virtual bool IsCritical() const override;
   virtual void Enter() override;
   virtual RecoveryAction Recover() override;
   virtual void Destroy() override;

   Test test_;
   signal_t signal_;
};

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_ctor = "RecoveryTestThread.ctor";

RecoveryTestThread::RecoveryTestThread() : Thread(PayloadFaction),
   test_(Sleep),
   signal_(0)
{
   Debug::ft(RecoveryTestThread_ctor);

   if(Debug::SwFlagOn(ThreadCtorTrapFlag)) UseBadPointer();
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_dtor = "RecoveryTestThread.dtor";

RecoveryTestThread::~RecoveryTestThread()
{
   Debug::ft(RecoveryTestThread_dtor);
}

//------------------------------------------------------------------------------

const char* RecoveryTestThread::AbbrName() const
{
   return "recov";
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_Destroy = "RecoveryTestThread.Destroy";

void RecoveryTestThread::Destroy()
{
   Debug::ft(RecoveryTestThread_Destroy);

   Singleton< RecoveryTestThread >::Destroy();
}

//------------------------------------------------------------------------------

void RecoveryTestThread::Display(ostream& stream,
   const string& prefix, const Flags& options) const
{
   Thread::Display(stream, prefix, options);

   stream << prefix << "test   : " << int(test_) << CRLF;
   stream << prefix << "signal : " << signal_ << CRLF;
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_Enter = "RecoveryTestThread.Enter";

void RecoveryTestThread::Enter()
{
   int n = 1, z = 0;

   while(true)
   {
      Debug::ft(RecoveryTestThread_Enter);

      //  Save and reset the test to be performed.  Otherwise, it will be
      //  immediately repeated upon reentering the thread after recovery.
      //
      auto test = test_;
      test_ = Sleep;

      //  Execute the requested test.
      //
      switch(test)
      {
      case Abort:
         std::abort();
         break;
      case Delete:
         Singleton< RecoveryTestThread >::Destroy();
         break;
      case DerefenceBadPtr:
         UseBadPointer();
         break;
      case DivideByZero:
         n = (n / z);
         break;
      case InfiniteLoop:
         LoopForever();
         break;
      case OverflowStack:
         RecurseForever(1);
         break;
      case RaiseSignal:
         raise(signal_);
         break;
      case Return:
         return;
      case Sleep:
         break;
      case Swerr:
         Debug::SwErr(RecoveryTestThread_Enter, test, 1, ErrorLog);
         break;
      case Terminate:
         std::terminate();
         break;
      case Trap:
         Raise(signal_);
         break;
      default:
         Debug::SwErr(RecoveryTestThread_Enter, test, 0);
      }

      //  Sleep for 3 seconds or until interrupted to perform the next test.
      //
      Pause(3 * TIMEOUT_1_SEC);
   }
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_IsCritical = "RecoveryTestThread.IsCritical";

bool RecoveryTestThread::IsCritical() const
{
   Debug::ft(RecoveryTestThread_IsCritical);

   return Debug::SwFlagOn(ThreadCriticalFlag);
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_LoopForever = "RecoveryTestThread.LoopForever";

void RecoveryTestThread::LoopForever()
{
   Debug::ft(RecoveryTestThread_LoopForever);

   while(true)
   {
      for(auto i = 0; i < 0x1000; ++i)
      {
         for(auto j = 0; j < 0x1000; ++j);
      }

      Debug::ft(RecoveryTestThread_LoopForever);
   }
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_Recover = "RecoveryTestThread.Recover";

Thread::RecoveryAction RecoveryTestThread::Recover()
{
   Debug::ft(RecoveryTestThread_Recover);

   if(Debug::SwFlagOn(ThreadRecoveryTrapFlag)) UseBadPointer();
   if(Debug::SwFlagOn(ThreadCriticalFlag)) return ReenterThread;
   return DeleteThread;
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_RecurseForever = "RecoveryTestThread.RecurseForever";

void RecoveryTestThread::RecurseForever(size_t depth)
{
   Debug::ft(RecoveryTestThread_RecurseForever);

   RecurseForever(depth + 1);
}

//------------------------------------------------------------------------------

fn_name RecoveryTestThread_UseBadPointer = "RecoveryTestThread.UseBadPointer";

void RecoveryTestThread::UseBadPointer()
{
   Debug::ft(RecoveryTestThread_UseBadPointer);

   //  Dereferencing P causes an exception.
   //
   auto p = reinterpret_cast< char* >(BAD_POINTER);
   if(*p == 0) ++p;
}

//------------------------------------------------------------------------------
//
//  The RECOVER command, for testing the Thread safety net.
//
class AbortText : public CliText
{
public: AbortText();
};

class BadPtrText : public CliText
{
public: BadPtrText();
};

class DeleteText : public CliText
{
public: DeleteText();
};

class DivideText : public CliText
{
public: DivideText();
};

class LoopText : public CliText
{
public: LoopText();
};

class RaiseText : public CliText
{
public: RaiseText();
};

class ReturnText : public CliText
{
public: ReturnText();
};

class SignalParm : public CliTextParm
{
public: SignalParm();
};

class StackText : public CliText
{
public: StackText();
};

class SwerrText : public CliText
{
public: SwerrText();
};

class TerminateText : public CliText
{
public: TerminateText();
};

class ThisParm : public CliBoolParm
{
public: ThisParm();
};

class TrapText : public CliText
{
public: TrapText();
};

class RecoverWhatParm : public CliTextParm
{
public: RecoverWhatParm();
};

class RecoverCommand : public CliCommand
{
public:
   RecoverCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

fixed_string AbortTextStr = "abort";
fixed_string AbortTextExpl = "call abort()";

AbortText::AbortText() : CliText(AbortTextExpl, AbortTextStr) { }

//------------------------------------------------------------------------------

fixed_string BadPtrTextStr = "badptr";
fixed_string BadPtrTextExpl = "dereference an invalid pointer";

BadPtrText::BadPtrText() : CliText(BadPtrTextExpl, BadPtrTextStr) { }

//------------------------------------------------------------------------------

fixed_string DeleteTextStr = "delete";
fixed_string DeleteTextExpl = "delete a thread";

DeleteText::DeleteText() : CliText(DeleteTextExpl, DeleteTextStr)
{
   BindParm(*new ThisParm);
}

//------------------------------------------------------------------------------

fixed_string DivideTextStr = "divide";
fixed_string DivideTextExpl = "divide by zero";

DivideText::DivideText() : CliText(DivideTextExpl, DivideTextStr) { }

//------------------------------------------------------------------------------

fixed_string LoopTextStr = "loop";
fixed_string LoopTextExpl = "enter an infinite loop";

LoopText::LoopText() : CliText(LoopTextExpl, LoopTextStr) { }

//------------------------------------------------------------------------------

fixed_string RaiseTextStr = "raise";
fixed_string RaiseTextExpl = "raise a signal";

RaiseText::RaiseText() : CliText(RaiseTextExpl, RaiseTextStr)
{
   BindParm(*new SignalParm);
}

//------------------------------------------------------------------------------

fixed_string ReturnTextStr = "return";
fixed_string ReturnTextExpl = "return from a thread";

ReturnText::ReturnText() : CliText(ReturnTextExpl, ReturnTextStr) { }

//------------------------------------------------------------------------------

fixed_string SignalParmExpl = "signal's name ('SIG...')";

SignalParm::SignalParm() : CliTextParm(SignalParmExpl) { }

//------------------------------------------------------------------------------

fixed_string StackTextStr = "stack";
fixed_string StackTextExpl = "cause a stack overflow";

StackText::StackText() : CliText(StackTextExpl, StackTextStr) { }

//------------------------------------------------------------------------------

fixed_string SwerrTextStr = "swerr";
fixed_string SwerrTextExpl = "cause a software abort log";

SwerrText::SwerrText() : CliText(SwerrTextExpl, SwerrTextStr) { }

//------------------------------------------------------------------------------

fixed_string TerminateTextStr = "terminate";
fixed_string TerminateTextExpl = "call terminate()";

TerminateText::TerminateText() :
   CliText(TerminateTextExpl, TerminateTextStr) { }

//------------------------------------------------------------------------------

fixed_string ThisParmExpl = "perform by 'this' (t) or by another thread (f)";

ThisParm::ThisParm() : CliBoolParm(ThisParmExpl) { }

//------------------------------------------------------------------------------

fixed_string TrapTextStr = "trap";
fixed_string TrapTextExpl = "cause a trap";

TrapText::TrapText() : CliText(TrapTextExpl, TrapTextStr)
{
   BindParm(*new ThisParm);
   BindParm(*new SignalParm);
}

//------------------------------------------------------------------------------

fixed_string RecoverWhatExpl = "what to recover from...";

RecoverWhatParm::RecoverWhatParm() : CliTextParm(RecoverWhatExpl)
{
   BindText(*new ReturnText, RecoveryTestThread::Return);
   BindText(*new AbortText, RecoveryTestThread::Abort);
   BindText(*new BadPtrText, RecoveryTestThread::DerefenceBadPtr);
   BindText(*new DivideText, RecoveryTestThread::DivideByZero);
   BindText(*new LoopText, RecoveryTestThread::InfiniteLoop);
   BindText(*new RaiseText, RecoveryTestThread::RaiseSignal);
   BindText(*new SwerrText, RecoveryTestThread::Swerr);
   BindText(*new TerminateText, RecoveryTestThread::Terminate);
   BindText(*new TrapText, RecoveryTestThread::Trap);
   BindText(*new StackText, RecoveryTestThread::OverflowStack);
   BindText(*new DeleteText, RecoveryTestThread::Delete);
}

//------------------------------------------------------------------------------

fixed_string RecoverStr = "recover";
fixed_string RecoverExpl = "Tests thread recovery.";

RecoverCommand::RecoverCommand() : CliCommand(RecoverStr, RecoverExpl)
{
   BindParm(*new RecoverWhatParm);
}

fn_name RecoverCommand_ProcessCommand = "RecoverCommand.ProcessCommand";

word RecoverCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(RecoverCommand_ProcessCommand);

   auto thr = Singleton< RecoveryTestThread >::Instance();
   auto reg = Singleton< PosixSignalRegistry >::Instance();
   id_t index;
   bool flag;
   string signame;
   signal_t signal;
   PosixSignal* ps;

   if(!Element::RunningInLab()) return cli.Report(-5, NotInFieldExpl);
   if(!GetTextIndex(index, cli)) return -1;

   switch(index)
   {
   case RecoveryTestThread::Abort:
   case RecoveryTestThread::DerefenceBadPtr:
   case RecoveryTestThread::DivideByZero:
   case RecoveryTestThread::InfiniteLoop:
   case RecoveryTestThread::OverflowStack:
   case RecoveryTestThread::Return:
   case RecoveryTestThread::Swerr:
   case RecoveryTestThread::Terminate:
      cli.EndOfInput(false);
      thr->SetTest(index);
      thr->Interrupt();
      break;

   case RecoveryTestThread::Delete:
      if(!GetBoolParm(flag, cli)) return -1;
      cli.EndOfInput(false);
      ThisThread::Pause(5 * TIMEOUT_1_SEC);
      if(flag)
      {
         thr->SetTest(index);
         thr->Interrupt();
      }
      else
      {
         Singleton< RecoveryTestThread >::Destroy();
      }
      break;

   case RecoveryTestThread::RaiseSignal:
      if(!GetString(signame, cli)) return -1;
      cli.EndOfInput(false);
      signal = reg->Value(signame);
      if(signal == SIGNIL) return cli.Report(-3, UnknownSignalExpl);
      thr->SetTest(index);
      thr->SetTestSignal(signal);
      thr->Interrupt();
      break;

   case RecoveryTestThread::Trap:
      if(!GetBoolParm(flag, cli)) return -1;
      if(!GetString(signame, cli)) return -1;
      cli.EndOfInput(false);
      ps = reg->Find(signame);
      if(ps == nullptr) return cli.Report(-3, UnknownSignalExpl);
      if(flag)
      {
         thr->SetTest(index);
         thr->SetTestSignal(ps->Value());
         thr->Interrupt();
      }
      else
      {
         thr->Raise(ps->Value());
      }
      break;

   default:
      Debug::SwErr(RecoverCommand_ProcessCommand, index, 0);
      return cli.Report(index, SystemErrorExpl);
   }

   return cli.Report(0, SuccessExpl);
}

//==============================================================================
//
//  The NodeBase tools and test increment.
//
fixed_string NtStr = "nt";
fixed_string NtExpl = "NodeBase Tools and Tests";

fn_name NtIncrement_ctor = "NtIncrement.ctor";

NtIncrement::NtIncrement() : CliIncrement(NtStr, NtExpl)
{
   Debug::ft(NtIncrement_ctor);

   BindCommand(*new NtSetCommand);
   BindCommand(*new NtSaveCommand);
   BindCommand(*new TestcaseCommand);
   BindCommand(*new SwFlagsCommand);
   BindCommand(*new SizesCommand);
   BindCommand(*new CorruptCommand);
   BindCommand(*new LeakyBucketCounterCommands);
   BindCommand(*new Q1WayCommands);
   BindCommand(*new Q2WayCommands);
   BindCommand(*new RegistryCommands);
   BindCommand(*new SysTimeCommands);
   BindCommand(*new RecoverCommand);
}

//------------------------------------------------------------------------------

fn_name NtIncrement_dtor = "NtIncrement.dtor";

NtIncrement::~NtIncrement()
{
   Debug::ft(NtIncrement_dtor);
}
}