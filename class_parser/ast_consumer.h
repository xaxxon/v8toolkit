#pragma once

#include "clang/AST/ASTConsumer.h"

#include "class_handler.h"

namespace v8toolkit::class_parser {


/**
 * Defines what will be matched and sent to
 */
class ClassHandlerASTConsumer : public clang::ASTConsumer {
private:

    // matcher that is parameterized in constructor
    ast_matchers::MatchFinder Matcher;

    // MatchCallback object called for each element matched by matcher
    ClassHandler class_handler;

    CompilerInstance & ci;

public:
    ClassHandlerASTConsumer(CompilerInstance & CI, vector<unique_ptr<OutputModule>> const & output_modules);

    void HandleTranslationUnit(ASTContext & Context) override {
        // Run the matchers when we have the whole TU parsed.
        // matchers configured in constructor
        Matcher.matchAST(Context);
    }


};

}
