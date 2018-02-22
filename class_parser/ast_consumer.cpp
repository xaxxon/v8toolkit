
#include "ast_consumer.h"

using namespace clang::ast_matchers;

namespace v8toolkit::class_parser {

ClassHandlerASTConsumer::ClassHandlerASTConsumer(CompilerInstance & ci,
                                                 vector<unique_ptr<OutputModule>> const & output_modules) :
    class_handler(ci, output_modules)
{

#ifdef TEMPLATE_INFO_ONLY

    Matcher.addMatcher(decl(anyOf(
                          classTemplateSpecializationDecl().bind("class"),
                          cxxMethodDecl().bind("method")
                          )),
                       &class_handler);

#else
    Matcher.addMatcher(cxxRecordDecl(
        allOf(
            // skip classes from v8toolkit::JSWrapper
            unless(isDerivedFrom("::v8toolkit::JSWrapper")), // JS-Classes will be completely regenerated

            // must be a struct or class
            anyOf(isStruct(), isClass()),
            unless(anyOf(matchesName("^::std::"), matchesName("^::eastl::"))),
            anyOf( // order of these matters.   If a class matches more than one it will only be returned as the first

                // things here will always be wrapped
                allOf(isDefinition(),
                      isDerivedFrom("::v8toolkit::WrappedClassBase"),
                      cxxRecordDecl().bind(
                          "class derived from WrappedClassBase")),

                // things here might be wrapped if they are a requirement from something else
                allOf(isDefinition(),
                      cxxRecordDecl().bind("not std:: class")),

                // used for applying attributes to types you don't control
                allOf(unless(isDefinition()),
                      cxxRecordDecl(hasAttr(attr::Annotate)).bind("forward declaration with annotation"))
            )
        )
    ), &class_handler);

    Matcher.addMatcher(
        namedDecl(
            allOf(
                hasAttr(attr::Annotate), // must have V8TOOLKIT_NAME_ALIAS set
                unless(matchesName("^::std::")),
                unless(matchesName("^::__")
                )
            )
        ).bind("named decl"), &class_handler);
#endif

}

}

