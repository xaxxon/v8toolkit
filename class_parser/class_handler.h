#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#pragma clang diagnostic pop

#include "output_modules.h"

namespace v8toolkit::class_parser {


class ClassHandler : public ast_matchers::MatchFinder::MatchCallback {
private:
    SourceManager & source_manager;

    std::set<std::string> names_used;
    vector<unique_ptr<OutputModule>> const & output_modules;

public:


    CompilerInstance & ci;

    /**
     * This runs per-match from ClassHandlerASTConsumer, but always on the same ClassHandler (this) object
     */
    virtual void run(const ast_matchers::MatchFinder::MatchResult & Result) override;


    ClassHandler(CompilerInstance & CI, vector<unique_ptr<OutputModule>> const & output_modules);

    ~ClassHandler();

    // all matcher callbacks have been run, now do anything to process the entirety of the data
    virtual void onEndOfTranslationUnit() override;

    // Run after compiling but before running the plugin over the contents of the AST
    virtual void onStartOfTranslationUnit() override;

};

}
