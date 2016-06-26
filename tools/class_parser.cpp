
// This clang plugin will look for classes marked as XXXX and generate c++ wrapper files for those classes and
//   automatically build up inheritence properly

// This program will only work with clang but the output should be useable on any platform.



/**
 * How to run over complete code base using cmake + cotire
add_library(api-gen-template OBJECT ${YOUR_SOURCE_FILES})
target_compile_options(api-gen-template
        PRIVATE -Xclang -ast-dump -fdiagnostics-color=never <== this isn't right yet, this just dumps the ast
        )
set_target_properties(api-gen-template PROPERTIES COTIRE_UNITY_TARGET_NAME "api-gen")
cotire(api-gen-template)
 *
 */




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

//#define PRINT_SKIPPED_EXPORT_REASONS true
#define PRINT_SKIPPED_EXPORT_REASONS false



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
        enum EXPORT_TYPE {
            EXPORT_UNSPECIFIED = 0,
            EXPORT_NONE, // export nothing
            EXPORT_SOME, // only exports specifically marked entities
            EXPORT_EXCEPT, // exports everything except specifically marked entities
            EXPORT_ALL}; // exports everything

//        CompilerInstance &CI;

        SourceManager & source_manager;
    public:

        EXPORT_TYPE get_export_type(const Decl * decl, EXPORT_TYPE previous = EXPORT_UNSPECIFIED) {
            auto &attrs = decl->getAttrs();
            EXPORT_TYPE export_type = previous;

            for (auto attr : attrs) {
                if (dyn_cast<AnnotateAttr>(attr)) {
                    auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                    auto string_ref = attribute_attr->getAnnotation();
//                        printf("annotation value: %s\n", string_ref.str().c_str());
                    if (string_ref.str() == "v8toolkit_generate_bindings_all") {
                        export_type = EXPORT_ALL;
                    } else if (string_ref.str() == "v8toolkit_generate_bindings_some") {
                        export_type = EXPORT_SOME;
                    } else if (string_ref.str() == "v8toolkit_generate_bindings_except") {
                        export_type = EXPORT_EXCEPT;
                    } else if (string_ref.str() == "v8toolkit_generate_bindings_none") {
                        export_type = EXPORT_NONE; // just for completeness
                    }
                }
            }
//            printf("Returning export type: %d\n", export_type);
            return export_type;
        }


        ClassHandler(CompilerInstance &CI) : source_manager(CI.getSourceManager())
        {}

        void handle_data_member(FieldDecl * field, EXPORT_TYPE parent_export_type, const std::string & indentation) {
            auto export_type = get_export_type(field, parent_export_type);
            auto field_name = field->getNameAsString();

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping data member %s because not supposed to be exported %d\n",
                       indentation.c_str(),
                       field_name.c_str(), export_type);
                return;
            }

            if (field->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), field_name.c_str());
                return;
            }

            printf("%sData member %s, type: %s\n",
                   indentation.c_str(),
                   field->getNameAsString().c_str(),
                   field->getType().getAsString().c_str());

        }

        void handle_method(CXXMethodDecl * method, EXPORT_TYPE parent_export_type, const std::string & indentation) {

            std::string method_name(method->getQualifiedNameAsString().c_str());
            auto export_type = get_export_type(method, parent_export_type);

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping method %s because not supposed to be exported %d\n",
                       indentation.c_str(), method_name.c_str(), export_type);
                return;
            }




            // only deal with public methods
            if (method->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), method_name.c_str());
                return;
            }
            if (method->isOverloadedOperator()) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping overloaded operator %s\n", indentation.c_str(), method_name.c_str());
                return;
            }
            if (dyn_cast<CXXConstructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping constructor %s\n", indentation.c_str(), method_name.c_str());
                return;
            }
            if (dyn_cast<CXXDestructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping destructor %s\n", indentation.c_str(), method_name.c_str());
                return;
            }
            if (method->isPure()) {
                assert(method->isVirtual());
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), method_name.c_str());
                return;
            }

            printf("%s", indentation.c_str());


            if (method->isStatic()) {
                printf("static ");
            }
            printf("%s %s(", method->getReturnType().getAsString().c_str(), method_name.c_str());

//            printf("static: %d instance: %d const: %d volatile: %d virtual: %d\n",
//                   method->isStatic(), method->isInstance(), method->isConst(), method->isVolatile(), method->isVirtual());


//            printf("Number input params: %d\n", method->getNumParams());
            bool first_param = true;
            for (unsigned int i = 0; i < method->getNumParams(); i++) {
                if (!first_param) {
                    printf(", ");
                }
                first_param = false;
                auto param = method->getParamDecl(i);
                printf("%s", param->getOriginalType().getAsString().c_str());
            }

            printf(")\n");
        }

        void handle_class(const CXXRecordDecl * klass,
                          EXPORT_TYPE parent_export_type = EXPORT_UNSPECIFIED,
                          const std::string & indentation = "") {
            auto class_name = klass->getQualifiedNameAsString();
            auto export_type = get_export_type(klass, parent_export_type);
            if (export_type == EXPORT_NONE) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping class %s marked EXPORT_NONE\n", indentation.c_str(), class_name.c_str());
                return;
            }

            // prints out source for decl
            //printf("class at %s", decl2str(klass,  source_manager).c_str());

//            auto full_source_loc = FullSourceLoc(klass->getLocation(), source_manager);
            printf("%sClass/struct: %s\n", indentation.c_str(), class_name.c_str());

//            printf("%s Decl at line %d, file id: %d\n", indentation.c_str(), full_source_loc.getExpansionLineNumber(),
//                   full_source_loc.getFileID().getHashValue());

//                auto type_decl = dyn_cast<TypeDecl>(klass);
//                assert(type_decl);
//                auto type = type_decl->getTypeForDecl();
//
            for(CXXMethodDecl * method : klass->methods()) {
                handle_method(method, export_type, indentation + "  ");
            }

            for (FieldDecl * field : klass->fields()) {
                handle_data_member(field, export_type, indentation + "  ");
            }


            for (auto base_class : klass->bases()) {
                auto qual_type = base_class.getType();
                auto record_decl = qual_type->getAsCXXRecordDecl();
                handle_class(record_decl, export_type, indentation + "  ");
            }
        }


        virtual void run(const MatchFinder::MatchResult &Result) {
            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class")) {
                handle_class(klass);
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