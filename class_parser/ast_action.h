#pragma once

#include <regex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendAction.h"
#pragma clang diagnostic pop

#include <xl/json.h>

#include "wrapped_class.h"

#include "ast_consumer.h"
#include "output_modules/javascript_stub_output.h"
#include "output_modules/bindings_output.h"
#include "output_modules/bidirectional_output.h"


namespace v8toolkit::class_parser {


// This is the class that is registered with LLVM.  PluginASTAction is-a ASTFrontEndAction
class PrintFunctionNamesAction : public clang::PluginASTAction {

protected:
    // The value returned here is used internally to run checks against
    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance & CI,
                                                   llvm::StringRef) override;

    bool ParseArgs(const CompilerInstance & CI,
                   const std::vector<std::string> & args) override;

    void PrintHelp(llvm::raw_ostream & ros);


public:

    static inline xl::json::Json config_data;


    // open up output files
    PrintFunctionNamesAction();
    ~PrintFunctionNamesAction();

    // This is called when all parsing is done
    void EndSourceFileAction() override;

    std::vector<std::unique_ptr<OutputModule>> output_modules;

    bool BeginInvocation(CompilerInstance & ci) override;

    void add_output_module(unique_ptr<OutputModule> output_module);

    /**
     * Returns Json object
     * @return
     */
    static xl::json::Json get_config_data();

};






}