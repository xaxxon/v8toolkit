#pragma once

#include "clang.h"
#include "wrapped_class.h"

#include "ast_consumer.h"

// This is the class that is registered with LLVM.  PluginASTAction is-a ASTFrontEndAction
class PrintFunctionNamesAction : public PluginASTAction {
public:

    // open up output files
    PrintFunctionNamesAction() {

    }

    // This is called when all parsing is done
    void EndSourceFileAction();

    // takes a file number starting at 1 and incrementing 1 each time
    // a list of WrappedClasses to print
    // and whether or not this is the last file to be written
    void write_classes(int file_count, vector<WrappedClass*> & classes, bool last_one);

protected:
    // The value returned here is used internally to run checks against
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   llvm::StringRef) {
        return llvm::make_unique<MyASTConsumer>(CI);
    }

    bool ParseArgs(const CompilerInstance &CI,
                   const std::vector<std::string> &args) {
        for (unsigned i = 0, e = args.size(); i != e; ++i) {
            llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

            // Example error handling.
            if (args[i] == "-an-error") {
                DiagnosticsEngine &D = CI.getDiagnostics();
                unsigned DiagID = D.getCustomDiagID(DiagnosticsEngine::Error,
                                                    "invalid argument '%0'");
                D.Report(DiagID) << args[i];
                return false;
            }
        }
        if (args.size() && args[0] == "help")
            PrintHelp(llvm::errs());

        return true;
    }
    void PrintHelp(llvm::raw_ostream &ros) {
        ros << "Help for PrintFunctionNames plugin goes here\n";
    }

};