#pragma once

#include "clang.h"

#include "class_handler.h"


/**
 * Defines what will be matched and sent to
 */
class MyASTConsumer : public ASTConsumer {
private:

    // matcher that is parameterized in constructor
    ast_matchers::MatchFinder Matcher;

    // MatchCallback object called for each element matched by matcher
    ClassHandler HandlerForClass;

    CompilerInstance & ci;

public:
    MyASTConsumer(CompilerInstance &CI);

    void HandleTranslationUnit(ASTContext &Context) override {
        // Run the matchers when we have the whole TU parsed.
        // matchers configured in constructor
        Matcher.matchAST(Context);
    }


};
