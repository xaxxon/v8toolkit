
// This clang plugin will look for classes marked as XXXX and generate c++ wrapper files for those classes and
//   automatically build up inheritence properly

// This program will only work with clang but the output should be useable on any platform.


#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;


namespace {

//    std::string decl2str(const clang::Decl *d, SourceManager &sm) {
//        // (T, U) => "T,,"
//        std::string text = Lexer::getSourceText(CharSourceRange::getTokenRange(d->getSourceRange()), sm, LangOptions(), 0);
//        if (text.at(text.size()-1) == ',')
//            return Lexer::getSourceText(CharSourceRange::getCharRange(d->getSourceRange()), sm, LangOptions(), 0);
//        return text;
//    }




    class ClassHandler : public MatchFinder::MatchCallback {
    private:
        CompilerInstance &CI;
    public:
        ClassHandler(CompilerInstance &CI) : CI(CI) {}
        virtual void run(const MatchFinder::MatchResult &Result) {
            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class")) {
                if (dyn_cast<NamedDecl>(klass)) {
                    auto named_class = dyn_cast<NamedDecl>(klass);

                    printf("Class/struct: %s has an annotation\n", named_class->getQualifiedNameAsString().c_str());
                }
                bool found_v8toolkit_generate_bindings = false;
                auto & attrs = klass->getAttrs();
                for (auto attr : attrs) {
                    if (dyn_cast<AnnotateAttr>(attr)) {
                        auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                        auto string_ref = attribute_attr->getAnnotation();
                        printf("annotation value: %s\n", string_ref.str().c_str());
                        if (string_ref.str() == "v8toolkit_generate_bindings") {
                            found_v8toolkit_generate_bindings = true;
                            break;
                        }
                    }
                }

                // if the specific annotation wasn't found, nothing to do here
                if (!found_v8toolkit_generate_bindings) {
                    printf("didn't find special binding name\n\n");
                    return;
                }

//                auto location = klass->getLocation();

                auto & source_manager = CI.getSourceManager();

                // prints out source for decl
                //printf("class at %s", decl2str(klass,  source_manager).c_str());

                auto full_source_loc = FullSourceLoc(klass->getLocation(), source_manager);
                printf("Decl at line %d, file id: %d\n", full_source_loc.getExpansionLineNumber(),
                       full_source_loc.getFileID().getHashValue());

//                auto type_decl = dyn_cast<TypeDecl>(klass);
//                assert(type_decl);
//                auto type = type_decl->getTypeForDecl();
//
                for(CXXMethodDecl * method : klass->methods()) {
                    auto named_decl = dyn_cast<NamedDecl>(method);
                    if (named_decl == nullptr) {
                        continue;
                    }
                    printf("Method: %s\n", named_decl->getQualifiedNameAsString().c_str());
                    printf("static: %d instance: %d const: %d volatile: %d virtual: %d\n",
                        method->isStatic(), method->isInstance(), method->isConst(), method->isVolatile(), method->isVirtual());

                    std::string access;
                    switch(method->getAccess()) {
                        case AS_public:
                            access = "public";
                            break;
                        case AS_private:
                            access = "private";
                            break;
                        case AS_protected:
                            access = "protected";
                            break;
                        case AS_none:
                            access = "none";
                            break;
                    }
                    printf("Access specifier: %s\n", access.c_str());
                    auto return_type = method->getReturnType();
                    printf("Return type: %s\n", return_type.getAsString().c_str());

                    printf("Number input params: %d\n", method->getNumParams());
                    for (unsigned int i = 0; i < method->getNumParams(); i++) {
                        auto param = method->getParamDecl(i);
                        printf("Parameter %d: %s\n", i, param->getOriginalType().getAsString().c_str());
                    }
                    printf("\n");
                }

                printf("\n");
            }
        }
    };


    // Implementation of the ASTConsumer interface for reading an AST produced
    // by the Clang parser. It registers a couple of matchers and runs them on
    // the AST.
    class MyASTConsumer : public ASTConsumer {
    public:
        MyASTConsumer(CompilerInstance &CI) : HandlerForClass(CI) {
            Matcher.addMatcher(cxxRecordDecl(anyOf(isStruct(), isClass()),
                                             hasAttr(attr::Annotate)).bind("class"),
                               //cxxRecordDecl(hasDeclaration(unless(namedDecl(isPrivate)))).bind("thing")
                &HandlerForClass);
        }

        void HandleTranslationUnit(ASTContext &Context) override {
            // Run the matchers when we have the whole TU parsed.
            Matcher.matchAST(Context);
        }

    private:

        ClassHandler HandlerForClass;
        MatchFinder Matcher;
    };



    // This is the class that is registered with LLVM.  PluginASTAction is-a ASTFrontEndAction
    class PrintFunctionNamesAction : public PluginASTAction {
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
}

static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
        X("print-fns", "print function names");