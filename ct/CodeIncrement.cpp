//==============================================================================
//
//  CodeIncrement.cpp
//
//  Copyright (C) 2012-2017 Greg Utas.  All rights reserved.
//
#include "CodeIncrement.h"
#include <cstddef>
#include <cstdio>
#include <iomanip>
#include <istream>
#include <sstream>
#include <string>
#include "CliBuffer.h"
#include "CliCharParm.h"
#include "CliCommand.h"
#include "CliIntParm.h"
#include "CliText.h"
#include "CliTextParm.h"
#include "CliThread.h"
#include "CodeDir.h"
#include "CodeFile.h"
#include "CodeTypes.h"
#include "Cxx.h"
#include "CxxRoot.h"
#include "Debug.h"
#include "Element.h"
#include "Formatters.h"
#include "Library.h"
#include "LibrarySet.h"
#include "NbCliParms.h"
#include "Registry.h"
#include "Singleton.h"
#include "Symbol.h"
#include "SysFile.h"
#include "SysTypes.h"

using std::setw;
using std::string;

//------------------------------------------------------------------------------

namespace CodeTools
{
//  Parameters used by more than one command.
//
class CodeSetExprParm : public CliTextParm
{
public: CodeSetExprParm();
};

class FileSetExprParm : public CliTextParm
{
public: FileSetExprParm();
};

class SetExprParm : public CliTextParm
{
public: SetExprParm();
};

class VarMandName : public CliTextParm
{
public: VarMandName();
};

fixed_string CodeSetExprExpl = "a set of code files or directories";

CodeSetExprParm::CodeSetExprParm() : CliTextParm(CodeSetExprExpl) { }

fixed_string FileSetExprExpl = "a set of code files";

FileSetExprParm::FileSetExprParm() : CliTextParm(FileSetExprExpl) { }

fixed_string SetExprExpl = "a set of code files or directories";

SetExprParm::SetExprParm() : CliTextParm(SetExprExpl) { }

fixed_string VarMandNameExpl = "variable name";

VarMandName::VarMandName() : CliTextParm(VarMandNameExpl) { }

//------------------------------------------------------------------------------
//
//  Base class for library commands that evaluate an expression.
//
class LibraryCommand : public CliCommand
{
public:
   //  Virtual to allow subclassing.
   //
   virtual ~LibraryCommand();

   //  Reads the rest of the input line and returns the result of
   //  evaluating it.
   //
   static LibrarySet* Evaluate(CliThread& cli);
protected:
   //  The arguments are from the base class.  Protected because this
   //  class is virtual.
   //
   LibraryCommand(const char* comm, const char* help);
};

LibraryCommand::LibraryCommand(const char* comm, const char* help) :
   CliCommand(comm, help) { }

LibraryCommand::~LibraryCommand() { }

fn_name LibraryCommand_Evaluate = "LibraryCommand.Evaluate";

LibrarySet* LibraryCommand::Evaluate(CliThread& cli)
{
   Debug::ft(LibraryCommand_Evaluate);

   string expr;

   auto pos = cli.Prompt().size() + cli.ibuf->Pos();
   cli.ibuf->Read(expr);
   cli.EndOfInput(false);

   auto result = Singleton< Library >::Instance()->Evaluate(expr, pos);
   return result;
}

//------------------------------------------------------------------------------
//
//  The APPLY command.
//
class ApplyFileParm : public CliTextParm
{
public: ApplyFileParm();
};

class ApplyCommand : public CliCommand
{
public:
   ApplyCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string ApplyFileExpl = "filename (assumed to end in \".trim.txt\")";

ApplyFileParm::ApplyFileParm() : CliTextParm(ApplyFileExpl) { }

fixed_string ApplyStr = "apply";
fixed_string ApplyExpl = "Interactively applies changes in a >trim file";

ApplyCommand::ApplyCommand() : CliCommand(ApplyStr, ApplyExpl)
{
   BindParm(*new ApplyFileParm);
}

fixed_string ApplyChars = "ynsq";
fixed_string ApplyHelp = "Enter y(yes) n(no) s(skip file) q(quit): ";

fn_name ApplyCommand_ProcessCommand = "ApplyCommand.ProcessCommand";

word ApplyCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ApplyCommand_ProcessCommand);

   string name, expl;

   if(!GetString(name, cli)) return -1;
   cli.EndOfInput(false);

   auto path = Element::OutputPath() + PATH_SEPARATOR + name + ".trim.txt";
   auto input = istreamPtr(SysFile::CreateIstream(path.c_str()));

   if(input == nullptr) return cli.Report(-7, "Failed to open file " + path);
   input->clear();
   input->seekg(0);

   auto act = CodeFile::NoChange;
   auto lib = Singleton< Library >::Instance();
   CodeFile* file = nullptr;
   string str, work, item, prep;
   word rc;

   while(input->peek() != EOF)
   {
      std::getline(*input, str);
      if(str.empty()) continue;

      auto pos = str.find_first_not_of(SPACE);

      switch(pos)
      {
      case 0:
         file = lib->FindFile(str);
         act = CodeFile::NoChange;
         if(file == nullptr) *cli.obuf << "Failed to find " << str << CRLF;
         break;

      case 3:
         if(file != nullptr)
         {
            work = str.substr(3);
            if(work == ADD_INCLUDE_STR) act = CodeFile::AddInclude;
            else if(work == REMOVE_INCLUDE_STR) act = CodeFile::RemoveInclude;
            else if(work == ADD_FORWARD_STR) act = CodeFile::AddForward;
            else if(work == REMOVE_FORWARD_STR) act = CodeFile::RemoveForward;
            else if(work == REMOVE_USING_STR) act = CodeFile::RemoveUsing;
            else act = CodeFile::NoChange;

            prep = (work.find("Add") == 0 ? "to" : "from");
         }
         break;

      case 6:
         if((file != nullptr) && (act != CodeFile::NoChange))
         {
            item = str.substr(6);
            expl = work + SPACE + item + SPACE;
            expl += prep + SPACE + file->Name() + '?';
            auto reply = cli.CharPrompt(expl, ApplyChars, ApplyHelp);
            expl.clear();

            switch(reply)
            {
            case 'y':
               rc = file->Modify(act, item, expl);
               *cli.obuf << spaces(2) << (rc >= 0 ? SuccessExpl : expl) << CRLF;
               break;
            case 'n':
               break;
            case 's':
               file = nullptr;
               break;
            case 'q':
               cli.Flush();
               return 0;
            }
         }
      }
   }

   cli.Flush();
   return 0;
}

//------------------------------------------------------------------------------
//
//  The ASSIGN command.
//
class AssignCommand : public CliCommand
{
public:
   AssignCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string AssignStr = "assign";
fixed_string AssignExpl =
   "Assigns a set of files or directories to a variable.";

AssignCommand::AssignCommand() : CliCommand(AssignStr, AssignExpl)
{
   BindParm(*new VarMandName);
   BindParm(*new CodeSetExprParm);
}

fn_name AssignCommand_ProcessCommand = "AssignCommand.ProcessCommand";

word AssignCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(AssignCommand_ProcessCommand);

   string name, expr, expl;

   if(!GetIdentifier(name, cli, Symbol::ValidNameChars(),
      Symbol::InvalidInitialChars())) return -1;
   auto pos = cli.Prompt().size() + cli.ibuf->Pos();
   cli.ibuf->Read(expr);
   cli.EndOfInput(false);

   auto rc = Singleton< Library >::Instance()->Assign(name, expr, pos, expl);
   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The CHECK command.
//
class CheckCommand : public LibraryCommand
{
public:
   CheckCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string CheckStr = "check";
fixed_string CheckExpl = "Checks if code follows C++ guidelines.";

CheckCommand::CheckCommand() : LibraryCommand(CheckStr, CheckExpl)
{
   BindParm(*new FileMandParm);
   BindParm(*new FileSetExprParm);
}

fn_name CheckCommand_ProcessCommand = "CheckCommand.ProcessCommand";

word CheckCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(CheckCommand_ProcessCommand);

   string title;

   if(!GetFileName(title, cli)) return -1;

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   auto stream = cli.FileStream();
   if(stream == nullptr) return cli.Report(-7, CreateStreamFailure);

   string expl;
   auto rc = set->Check(*stream, expl);
   set->Release();

   if(rc == 0)
   {
      title += ".check.txt";
      cli.SendToFile(title, true);
   }

   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The COUNT command.
//
class CountCommand : public LibraryCommand
{
public:
   CountCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string CountStr = "count";
fixed_string CountExpl = "Counts the items in a set.";

CountCommand::CountCommand() : LibraryCommand(CountStr, CountExpl)
{
   BindParm(*new SetExprParm);
}

fn_name CountCommand_ProcessCommand = "CountCommand.ProcessCommand";

word CountCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(CountCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string result;

   auto rc = set->Count(result);
   set->Release();
   return cli.Report(rc, result);
}

//------------------------------------------------------------------------------
//
//  The COUNTLINES command.
//
class CountlinesCommand : public LibraryCommand
{
public:
   CountlinesCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string CountlinesStr = "countlines";
fixed_string CountlinesExpl = "Counts the number of lines of code.";

CountlinesCommand::CountlinesCommand() :
   LibraryCommand(CountlinesStr, CountlinesExpl)
{
   BindParm(*new FileSetExprParm);
}

fn_name CountlinesCommand_ProcessCommand = "CountlinesCommand.ProcessCommand";

word CountlinesCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(CountlinesCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string result;

   auto rc = set->Countlines(result);
   set->Release();
   return cli.Report(rc, result);
}

//------------------------------------------------------------------------------
//
//  The EXPORT command.
//
class OrderParm : public CliCharParm
{
public: OrderParm();
};

fixed_string OrderStr = "co";
fixed_string OrderExpl = "'c'=canonical 'o'=original (default='c')";

OrderParm::OrderParm() : CliCharParm(OrderExpl, OrderStr, true) { }

class ExportCommand : public CliCommand
{
public:
   ExportCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string ExportStr = "export";
fixed_string ExportExpl = "Exports library information.";

ExportCommand::ExportCommand() : CliCommand(ExportStr, ExportExpl)
{
   BindParm(*new FileMandParm);
   BindParm(*new OrderParm);
}

fn_name ExportCommand_ProcessCommand = "ExportCommand.ProcessCommand";

word ExportCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ExportCommand_ProcessCommand);

   string title;

   if(!GetFileName(title, cli)) return -1;
   cli.EndOfInput(false);

   auto stream = cli.FileStream();
   if(stream == nullptr) return cli.Report(-7, CreateStreamFailure);

   string opts;
   char c;
   if(GetCharParmRc(c, cli) == CliParm::Ok)
      opts = c;
   else
      opts = "c";

   auto lib = Singleton< Library >::Instance();
   auto rc = lib->Export(*stream, opts);

   if(rc == 0)
   {
      title += ".lib.txt";
      cli.SendToFile(title, true);
   }

   return rc;
}

//------------------------------------------------------------------------------
//
//  The FILEID command.
//
class FileIdMandParm : public CliIntParm
{
public: FileIdMandParm();
};

class FileIdCommand : public CliCommand
{
public:
   FileIdCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string FileIdMandExpl = "file's identifier";

FileIdMandParm::FileIdMandParm() :
   CliIntParm(FileIdMandExpl, 1, 4095) { }

fixed_string FileIdStr = "fileid";
fixed_string FileIdExpl = "Displays information about a code file.";

FileIdCommand::FileIdCommand() : CliCommand(FileIdStr, FileIdExpl)
{
   BindParm(*new FileIdMandParm);
}

fn_name FileIdCommand_ProcessCommand = "FileIdCommand.ProcessCommand";

word FileIdCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(FileIdCommand_ProcessCommand);

   word fid;

   if(!GetIntParm(fid, cli)) return -1;
   cli.EndOfInput(false);

   auto file = Singleton< Library >::Instance()->Files().At(fid);
   if(file == nullptr) return cli.Report(-2, NoFileExpl);
   file->Display(*cli.obuf, spaces(2), Flags(Vb_Mask));
   return 0;
}

//------------------------------------------------------------------------------
//
//  The FILEINFO command.
//
class CodeFileParm : public CliTextParm
{
public: CodeFileParm();
};

class FileInfoCommand : public CliCommand
{
public:
   FileInfoCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string CodeFileExpl = "filename (including extension)";

CodeFileParm::CodeFileParm() : CliTextParm(CodeFileExpl) { }

fixed_string FileInfoStr = "fileinfo";
fixed_string FileInfoExpl = "Displays information about a code file.";

FileInfoCommand::FileInfoCommand() : CliCommand(FileInfoStr, FileInfoExpl)
{
   BindParm(*new CodeFileParm);
}

fn_name FileInfoCommand_ProcessCommand = "FileInfoCommand.ProcessCommand";

word FileInfoCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(FileInfoCommand_ProcessCommand);

   string name;

   if(!GetString(name, cli)) return -1;
   cli.EndOfInput(false);

   auto file = Singleton< Library >::Instance()->FindFile(name);
   if(file == nullptr) return cli.Report(-2, NoFileExpl);
   file->Display(*cli.obuf, spaces(2), Flags(Vb_Mask));
   return 0;
}

//------------------------------------------------------------------------------
//
//  The FORMAT command.
//
class FormatCommand : public LibraryCommand
{
public:
   FormatCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string FormatStr = "format";
fixed_string FormatExpl = "Reformats code files.";

FormatCommand::FormatCommand() : LibraryCommand(FormatStr, FormatExpl)
{
   BindParm(*new CodeSetExprParm);
}

fn_name FormatCommand_ProcessCommand = "FormatCommand.ProcessCommand";

word FormatCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(FormatCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string expl;

   auto rc = set->Format(expl);
   set->Release();
   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The IMPORT command.
//
class DirMandName : public CliTextParm
{
public: DirMandName();
};

class PathOptParm : public CliTextParm
{
public: PathOptParm();
};

class ImportCommand : public CliCommand
{
public:
   ImportCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string DirMandNameExpl = "directory name";

DirMandName::DirMandName() : CliTextParm(DirMandNameExpl) { }

fixed_string PathOptExpl = "path within SourcePath configuration parameter";

PathOptParm::PathOptParm() : CliTextParm(PathOptExpl, true) { }

fixed_string ImportStr = "import";
fixed_string ImportExpl = "Adds a directory to the code base.";

ImportCommand::ImportCommand() : CliCommand(ImportStr, ImportExpl)
{
   BindParm(*new DirMandName);
   BindParm(*new PathOptParm);
}

fn_name ImportCommand_ProcessCommand = "ImportCommand.ProcessCommand";

word ImportCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ImportCommand_ProcessCommand);

   string name, subdir, expl;

   if(!GetIdentifier(name, cli, Symbol::ValidNameChars(),
      Symbol::InvalidInitialChars())) return -1;
   GetStringRc(subdir, cli);
   cli.EndOfInput(false);

   auto path = Library::SourcePath();
   if(!subdir.empty()) path += PATH_SEPARATOR + subdir;
   auto rc = Singleton< Library >::Instance()->Import(name, path, expl);
   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The LIST command.
//
class ListCommand : public LibraryCommand
{
public:
   ListCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string ListStr = "list";
fixed_string ListExpl = "Displays the items in a set, one per line.";

ListCommand::ListCommand() : LibraryCommand(ListStr, ListExpl)
{
   BindParm(*new CodeSetExprParm);
}

fn_name ListCommand_ProcessCommand = "ListCommand.ProcessCommand";

word ListCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ListCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string expl;

   auto rc = set->List(*cli.obuf, expl);
   set->Release();
   if(rc != 0) return cli.Report(rc, expl);
   return rc;
}

//------------------------------------------------------------------------------
//
//  The PARSE command.
//
class OptionsParm : public CliTextParm
{
public: OptionsParm();
};

fixed_string OptionsExpl = "parser options (enter >help full for details)";

OptionsParm::OptionsParm() : CliTextParm(OptionsExpl) { }

class DefineFileParm : public CliTextParm
{
public: DefineFileParm();
};

fixed_string DefineFileExpl = "file for #define symbols (.txt in input directory)";

DefineFileParm::DefineFileParm() : CliTextParm(DefineFileExpl) { }

class ParseCommand : public LibraryCommand
{
public:
   ParseCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string ParseStr = "parse";
fixed_string ParseExpl = "Parses code files.";

ParseCommand::ParseCommand() : LibraryCommand(ParseStr, ParseExpl)
{
   BindParm(*new OptionsParm);
   BindParm(*new DefineFileParm);
   BindParm(*new FileSetExprParm);
}

fn_name ParseCommand_ProcessCommand = "ParseCommand.ProcessCommand";

word ParseCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ParseCommand_ProcessCommand);

   string opts;
   string name;
   string expl;

   if(!GetString(opts, cli)) return -1;

   if(!GetString(name, cli)) return -1;
   auto path = Element::InputPath() + PATH_SEPARATOR + name + ".txt";
   auto file = SysFile::CreateIstream(path.c_str());
   if(file == nullptr) return cli.Report(-2, NoFileExpl);
   Singleton< CxxRoot >::Instance()->DefineSymbols(*file.get());

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   auto rc = set->Parse(expl, opts);
   set->Release();
   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The PURGE command.
//
class PurgeCommand : public CliCommand
{
public:
   PurgeCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string PurgeStr = "purge";
fixed_string PurgeExpl = "Deletes a variable.";

PurgeCommand::PurgeCommand() : CliCommand(PurgeStr, PurgeExpl)
{
   BindParm(*new VarMandName);
}

fn_name PurgeCommand_ProcessCommand = "PurgeCommand.ProcessCommand";

word PurgeCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(PurgeCommand_ProcessCommand);

   string name, expr, expl;

   if(!GetIdentifier(name, cli, Symbol::ValidNameChars(),
      Symbol::InvalidInitialChars())) return -1;
   cli.EndOfInput(false);

   auto rc = Singleton< Library >::Instance()->Purge(name, expl);
   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The SCAN command.
//
   class StringPatternParm : public CliTextParm
   {
   public: StringPatternParm();
   };

   class ScanCommand : public CliCommand
   {
   public:
      ScanCommand();
   private:
      virtual word ProcessCommand(CliThread& cli) const override;
   };

fixed_string StringPatternExpl = "string to look for (quoted; '$' = wildcard)";

StringPatternParm::StringPatternParm() : CliTextParm(StringPatternExpl) { }

fixed_string ScanStr = "scan";
fixed_string ScanExpl = "Scans files for lines that contain a string.";

ScanCommand::ScanCommand() : CliCommand(ScanStr, ScanExpl)
{
   BindParm(*new FileSetExprParm);
   BindParm(*new StringPatternParm);
}

fn_name ScanCommand_ProcessCommand = "ScanCommand.ProcessCommand";

word ScanCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ScanCommand_ProcessCommand);

   string line, expr, pattern, expl;

   //  Read the entire line and then extract the quoted string at the end.
   //
   auto pos = cli.Prompt().size() + cli.ibuf->Pos();
   cli.ibuf->Read(line);
   cli.EndOfInput(false);

   auto quote1 = line.find(QUOTE);
   auto quote2 = line.rfind(QUOTE);
   if(quote1 == string::npos) return cli.Report(-2, "Quoted string missing.");
   if(quote2 == quote1) return cli.Report(-2, "Closing \" missing.");
   if(quote2 - quote1 == 1) return cli.Report(-2, "Pattern string is empty.");

   expr = line.substr(0, quote1);
   pattern = line.substr(quote1 + 1, quote2 - (quote1 + 1));

   auto set = Singleton< Library >::Instance()->Evaluate(expr, pos);
   if(set == nullptr) return cli.Report(-7, AllocationError);
   auto rc = set->Scan(*cli.obuf, pattern, expl);
   set->Release();
   if(rc != 0) return cli.Report(rc, expl);
   return rc;
}

//------------------------------------------------------------------------------
//
//  The SHOW command.
//
class DirsText : public CliText
{
public: DirsText();
};

class FailedText : public CliText
{
public: FailedText();
};

class ItemsText : public CliText
{
public: ItemsText();
};

class ShowWhatParm : public CliTextParm
{
public: ShowWhatParm();
};

class ShowCommand : public LibraryCommand
{
public:
   static const id_t DirsIndex = 1;
   static const id_t FailedIndex = 2;
   static const id_t ItemsIndex = 3;

   ShowCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string DirsTextStr = "dirs";
fixed_string DirsTextExpl = "code directories";

DirsText::DirsText() : CliText(DirsTextExpl, DirsTextStr) { }

fixed_string FailedTextStr = "failed";
fixed_string FailedTextExpl = "code files that failed to parse";

FailedText::FailedText() : CliText(FailedTextExpl, FailedTextStr) { }

fixed_string ItemsTextStr = "items";
fixed_string ItemsTextExpl = "memory usage by item type";

ItemsText::ItemsText() : CliText(ItemsTextExpl, ItemsTextStr) { }

fixed_string ShowWhatExpl = "what to show...";

ShowWhatParm::ShowWhatParm() : CliTextParm(ShowWhatExpl)
{
   BindText(*new DirsText, ShowCommand::DirsIndex);
   BindText(*new FailedText, ShowCommand::FailedIndex);
   BindText(*new ItemsText, ShowCommand::ItemsIndex);
}

fixed_string ShowStr = "show";
fixed_string ShowExpl = "Displays library information.";

ShowCommand::ShowCommand() : LibraryCommand(ShowStr, ShowExpl)
{
   BindParm(*new ShowWhatParm);
}

fn_name ShowCommand_ProcessCommand = "ShowCommand.ProcessCommand";

word ShowCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ShowCommand_ProcessCommand);

   id_t index;

   if(!GetTextIndex(index, cli)) return -1;
   cli.EndOfInput(false);

   switch(index)
   {
   case DirsIndex:
      {
         //  Display the number of .h and .cpp files found in each directory.
         //
         *cli.obuf << "  Directory    .h  .cpp  Path" << CRLF;

         size_t hdrs = 0;
         size_t cpps = 0;
         auto& dirs = Singleton< Library >::Instance()->Directories();

         for(auto d = dirs.First(); d != nullptr; dirs.Next(d))
         {
            auto h = d->HeaderCount();
            *cli.obuf << setw(11) << d->Name();
            *cli.obuf << setw(6) << h;
            auto c = d->CppCount();
            *cli.obuf << setw(6) << c;
            *cli.obuf << spaces(2) << d->Path() << CRLF;
            hdrs += h;
            cpps += c;
         }

         *cli.obuf << setw(11) << "TOTAL" << setw(6) << hdrs
            << setw(6) << cpps << CRLF;
      }
      break;

   case FailedIndex:
      {
         auto found = false;
         auto& files = Singleton< Library >::Instance()->Files();

         for(auto f = files.First(); f != nullptr; files.Next(f))
         {
            if(f->ParseStatus() == CodeFile::Failed)
            {
               *cli.obuf << spaces(2) << f->Name() << CRLF;
               found = true;
            }
         }

         if(!found) return cli.Report(0, "No files failed to parse.");
      }
      break;

   case ItemsIndex:
      CxxStats::Display(*cli.obuf);
      break;

   default:
      Debug::SwErr(ShowCommand_ProcessCommand, index, 0);
      return cli.Report(index, SystemErrorExpl);
   }

   return 0;
}

//------------------------------------------------------------------------------
//
//  The SHRINK command.
//
class ShrinkCommand : public CliCommand
{
public:
   ShrinkCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

fixed_string ShrinkStr = "shrink";
fixed_string ShrinkExpl = "Shrinks the library's element containers.";

ShrinkCommand::ShrinkCommand() : CliCommand(ShrinkStr, ShrinkExpl) { }

fn_name ShrinkCommand_ProcessCommand = "ShrinkCommand.ProcessCommand";

word ShrinkCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ShrinkCommand_ProcessCommand);

   cli.EndOfInput(false);

   CxxStats::Shrink();
   return 0;
}

//------------------------------------------------------------------------------
//
//  The SORT command.
//
class SortCommand : public LibraryCommand
{
public:
   SortCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string SortStr = "sort";
fixed_string SortExpl = "Sorts files by build dependency order.";

SortCommand::SortCommand() : LibraryCommand(SortStr, SortExpl)
{
   BindParm(*new FileSetExprParm);
}

fn_name SortCommand_ProcessCommand = "SortCommand.ProcessCommand";

word SortCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(SortCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string expl;

   auto rc = set->Sort(*cli.obuf, expl);
   set->Release();
   if(rc != 0) return cli.Report(rc, expl);
   return rc;
}

//------------------------------------------------------------------------------
//
//  The TRIM command.
//
class TrimCommand : public LibraryCommand
{
public:
   TrimCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string TrimStr = "trim";
fixed_string TrimExpl = "Analyzes #include and using statements.";

TrimCommand::TrimCommand() : LibraryCommand(TrimStr, TrimExpl)
{
   BindParm(*new FileMandParm);
   BindParm(*new FileSetExprParm);
}

fn_name TrimCommand_ProcessCommand = "TrimCommand.ProcessCommand";

word TrimCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TrimCommand_ProcessCommand);

   string title;

   if(!GetFileName(title, cli)) return -1;

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   auto stream = cli.FileStream();
   if(stream == nullptr) return cli.Report(-7, CreateStreamFailure);

   string expl;
   auto rc = set->Trim(*stream, expl);
   set->Release();

   if(rc == 0)
   {
      title += ".trim.txt";
      cli.SendToFile(title, true);
   }

   return cli.Report(rc, expl);
}

//------------------------------------------------------------------------------
//
//  The TYPE command.
//
class TypeCommand : public LibraryCommand
{
public:
   TypeCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

fixed_string TypeStr = "type";
fixed_string TypeExpl = "Displays the items in a set, separated by commas.";

TypeCommand::TypeCommand() : LibraryCommand(TypeStr, TypeExpl)
{
   BindParm(*new SetExprParm);
}

fn_name TypeCommand_ProcessCommand = "TypeCommand.ProcessCommand";

word TypeCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(TypeCommand_ProcessCommand);

   auto set = LibraryCommand::Evaluate(cli);
   if(set == nullptr) return cli.Report(-7, AllocationError);

   string result;

   auto rc = set->Show(result);
   set->Release();
   return cli.Report(rc, result);
}

//------------------------------------------------------------------------------
//
//  Command for experimental testing.
//
class ExpCommand : public CliCommand
{
public:
   ExpCommand();
private:
   virtual word ProcessCommand(CliThread& cli) const override;
};

//------------------------------------------------------------------------------

fixed_string ExpStr = "exp";
fixed_string ExpExpl = "Performs an experimental test.";

ExpCommand::ExpCommand() : CliCommand(ExpStr, ExpExpl) { }

fn_name ExpCommand_ProcessCommand = "ExpCommand.ProcessCommand";

word ExpCommand::ProcessCommand(CliThread& cli) const
{
   Debug::ft(ExpCommand_ProcessCommand);

   cli.EndOfInput(false);
   *cli.obuf << "This command currently does nothing." << CRLF;
   return 0;
}

//------------------------------------------------------------------------------
//
//  The source code increment.
//
fixed_string CtStr = "ct";
fixed_string CtExpl = "CodeTools Increment";

fn_name CodeIncrement_ctor = "CodeIncrement.ctor";

CodeIncrement::CodeIncrement() : CliIncrement(CtStr, CtExpl)
{
   Debug::ft(CodeIncrement_ctor);

   BindCommand(*new ImportCommand);
   BindCommand(*new ShowCommand);
   BindCommand(*new TypeCommand);
   BindCommand(*new ListCommand);
   BindCommand(*new CountCommand);
   BindCommand(*new CountlinesCommand);
   BindCommand(*new ScanCommand);
   BindCommand(*new AssignCommand);
   BindCommand(*new PurgeCommand);
   BindCommand(*new SortCommand);
   BindCommand(*new FileInfoCommand);
   BindCommand(*new FileIdCommand);
   BindCommand(*new ParseCommand);
   BindCommand(*new CheckCommand);
   BindCommand(*new TrimCommand);
   BindCommand(*new ApplyCommand);
   BindCommand(*new FormatCommand);
   BindCommand(*new ExportCommand);
   BindCommand(*new ShrinkCommand);
   BindCommand(*new ExpCommand);
}

//------------------------------------------------------------------------------

fn_name CodeIncrement_dtor = "CodeIncrement.dtor";

CodeIncrement::~CodeIncrement()
{
   Debug::ft(CodeIncrement_dtor);
}
}