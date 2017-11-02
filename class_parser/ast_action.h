#pragma once

#include <regex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendAction.h"
#pragma clang diagnostic pop

#include "wrapped_class.h"

#include "ast_consumer.h"
#include "output_modules/javascript_stub_output.h"
#include "output_modules/bindings_output.h"
#include "output_modules/bidirectional_output.h"


namespace v8toolkit::class_parser {


// This is the class that is registered with LLVM.  PluginASTAction is-a ASTFrontEndAction
class PrintFunctionNamesAction : public clang::PluginASTAction {
public:

    // open up output files
    PrintFunctionNamesAction() {
        WrappedClass::wrapped_classes.clear();
        WrappedClass::used_constructor_names.clear();

    }
    ~PrintFunctionNamesAction() {}

    // This is called when all parsing is done
    void EndSourceFileAction() override;

    vector<unique_ptr<OutputModule>> output_modules;

    bool BeginInvocation(CompilerInstance & ci) override;

protected:
    // The value returned here is used internally to run checks against
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance & CI,
                                                   llvm::StringRef) override {
        return llvm::make_unique<ClassHandlerASTConsumer>(CI, this->output_modules);
    }

    bool ParseArgs(const CompilerInstance & CI,
                   const std::vector<std::string> & args) override {
        for (unsigned i = 0, e = args.size(); i < e; ++i) {
            llvm::errs() << "PrintFunctionNames arg = " << args[i] << "\n";

            std::regex declaration_count_regex("^--declaration-count=(\\d+)$");
            std::smatch match_results;
            if (std::regex_match(args[i], match_results, declaration_count_regex)) {
                auto count = std::stoi(match_results[1].str());
                std::cerr << fmt::format("Set declaration count to {}", count) << std::endl;
                MAX_DECLARATIONS_PER_FILE = count;
            }
            // for "normal" use, the default output modules should be used, instead of others specified
            //   in code from something such as a test harness
            else if (args[i] == "--use-default-output-modules") {
                std::cerr << fmt::format("Using default output modules") << std::endl;
                output_modules.push_back(std::make_unique<javascript_stub_output::JavascriptStubOutputModule>());
            }
        }
        if (args.size() && args[0] == "help")
            PrintHelp(llvm::errs());

        return true;
    }

    void PrintHelp(llvm::raw_ostream & ros) {
        ros << "Help for PrintFunctionNames plugin goes here\n";
    }

public:

    void add_output_module(unique_ptr<OutputModule> output_module) {
        this->output_modules.push_back(std::move(output_module));
    }

};




}