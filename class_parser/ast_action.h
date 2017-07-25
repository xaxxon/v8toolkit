#pragma once

#include <regex>

#include "clang.h"
#include "wrapped_class.h"

#include "ast_consumer.h"

namespace v8toolkit::class_parser {


// This is the class that is registered with LLVM.  PluginASTAction is-a ASTFrontEndAction
class PrintFunctionNamesAction : public clang::PluginASTAction {
public:

    // open up output files
    PrintFunctionNamesAction() {

    }

    // This is called when all parsing is done
    void EndSourceFileAction();


protected:
    // The value returned here is used internally to run checks against
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance & CI,
                                                   llvm::StringRef) {
        return llvm::make_unique<MyASTConsumer>(CI);
    }

    bool ParseArgs(const CompilerInstance & CI,
                   const std::vector<std::string> & args) {
        for (unsigned i = 0, e = args.size(); i < e; ++i) {
            llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

            std::regex declaration_count_regex("^--declaration-count=(\\d+)$");
            std::smatch match_results;
            if (std::regex_match(args[i], match_results, declaration_count_regex)) {
                auto count = std::stoi(match_results[1].str());
                MAX_DECLARATIONS_PER_FILE = count;
            }
        }
        if (args.size() && args[0] == "help")
            PrintHelp(llvm::errs());

        return true;
    }

    void PrintHelp(llvm::raw_ostream & ros) {
        ros << "Help for PrintFunctionNames plugin goes here\n";
    }

};


}