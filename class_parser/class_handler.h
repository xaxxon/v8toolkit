#pragma once

#include "bidirectional_bindings.h"

#define PRINT_SKIPPED_EXPORT_REASONS false
//#define PRINT_SKIPPED_EXPORT_REASONS false

#include "helper_functions.h"

class ClassHandler : public ast_matchers::MatchFinder::MatchCallback {
private:
    SourceManager & source_manager;

    WrappedClass * top_level_class; // the class currently being wrapped
    std::set<std::string> names_used;

public:

    CompilerInstance & ci;

    /**
     * This runs per-match from MyASTConsumer, but always on the same ClassHandler (this) object
     */
    virtual void run(const ast_matchers::MatchFinder::MatchResult &Result) override;


    ClassHandler(CompilerInstance &CI) :
        ci(CI),
        source_manager(CI.getSourceManager())
    {}


    // all matcher callbacks have been run, now do anything to process the entirety of the data
    virtual  void onEndOfTranslationUnit () override;


    std::string handle_data_member(WrappedClass & containing_class, FieldDecl * field, const std::string & indentation);


    void handle_class(WrappedClass & wrapped_class, // class currently being handled (not necessarily top level)
                      bool top_level = true,
                      const std::string & indentation = "");



};
