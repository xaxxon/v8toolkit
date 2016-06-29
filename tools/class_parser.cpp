
// This clang plugin will look for classes marked as XXXX and generate c++ wrapper files for those classes and
//   automatically build up inheritence properly

// This program will only work with clang but the output should be useable on any platform.



/**
How to run over complete code base using cmake + cotire
add_library(api-gen-template OBJECT ${YOUR_SOURCE_FILES})
target_compile_options(api-gen-template
        PRIVATE -Xclang -ast-dump -fdiagnostics-color=never <== this isn't right yet, this just dumps the ast
        )
set_target_properties(api-gen-template PROPERTIES COTIRE_UNITY_TARGET_NAME "api-gen")
cotire(api-gen-template)
 */


/**
 * THIS IS HIGHLY EXPERIMENTAL - IT PROBABLY DOESN'T WORK AT ALL
 *
 * Special attribute names recognized:
 *
 * wrapper bindings:
 * __attribute__((annotate("v8toolkit_generate_bindings_none"))) // skip this class/method/member
 * __attribute__((annotate("v8toolkit_generate_bindings_some"))) // not sure if this actually works
 * __attribute__((annotate("v8toolkit_generate_bindings_except"))) // not sure if this actually works
 * __attribute__((annotate("v8toolkit_generate_bindings_all"))) // export everything under this by default unless marked as none
 *
 * bidirectional bindings:
 * __attribute__((annotate("v8toolkit_generate_bidirectional")))
 */

#include <iostream>
#include <sstream>

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

using namespace std;

//#define PRINT_SKIPPED_EXPORT_REASONS true
#define PRINT_SKIPPED_EXPORT_REASONS false



namespace {

    // returns a vector of all the annotations on a Decl
    std::vector<std::string> get_annotations(const Decl * decl) {
        std::vector<std::string> results;
        for (auto attr : decl->getAttrs()) {
            AnnotateAttr * annotation =  dyn_cast<AnnotateAttr>(attr);
            if (annotation) {
                auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                auto annotation_string = attribute_attr->getAnnotation().str();
                results.emplace_back(annotation->getAnnotation().str());
            }
        }
        return results;
    }

    bool has_annotation(const Decl * decl, const std::string & target) {
        auto annotations = get_annotations(decl);
        return std::find(annotations.begin(), annotations.end(), target) != annotations.end();
    }



    class BidirectionalBindings {
    private:
        const CXXRecordDecl * starting_class;

    public:
        BidirectionalBindings(const CXXRecordDecl * starting_class) : starting_class(starting_class){}
        std::string short_name(){return starting_class->getName();}

        void print_virtual(const CXXMethodDecl * method) {

            std::stringstream result;


            if (method->isConst()) {
                result << "  JS_ACCESS_CONST_";
            } else {
                result << "  JS_ACCESS_";
            }
            auto num_params = method->getNumParams();
            result << num_params << "(";

            auto return_type_string = method->getReturnType().getAsString();
            result << return_type_string << ", ";

            auto method_name = method->getName();
            result << method_name.str() << ");";



            cout << result.str() << endl;

        }

        void handle_class(const CXXRecordDecl * klass) {
            for(CXXMethodDecl * method : klass->methods()) {
                if (method->isVirtual()) {
                    print_virtual(method);
                }
            }

            for (auto base_class : klass->bases()) {
                auto base_decl = base_class.getType()->getAsCXXRecordDecl();
                handle_class(base_decl);
            }

        }

        void print_bindings() {
            auto annotations = get_annotations(starting_class);
            if (has_annotation(starting_class, "v8toolkit_generate_bidirectional")) {
                printf("class JS%s : public %s, public v8toolkit::JSWrapper<%s> {\n",
                    short_name().c_str(), short_name().c_str(), short_name().c_str());
                handle_class(starting_class);
                printf("};\n");
            } else {
//                printf("Class %s not marked bidirectional\n", short_name().c_str());
            }
        }
    };



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
                    auto annotation_string = attribute_attr->getAnnotation().str();
                    if (annotation_string == "v8toolkit_generate_bindings_all") {
                        export_type = EXPORT_ALL;
                    } else if (annotation_string == "v8toolkit_generate_bindings_some") {
                        export_type = EXPORT_SOME;
                    } else if (annotation_string == "v8toolkit_generate_bindings_except") {
                        export_type = EXPORT_EXCEPT;
                    } else if (annotation_string == "v8toolkit_generate_bindings_none") {
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
            auto short_field_name = field->getNameAsString();
            auto full_field_name = field->getQualifiedNameAsString();


            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping data member %s because not supposed to be exported %d\n",
                       indentation.c_str(),
                       short_field_name.c_str(), export_type);
                return;
            }

            if (field->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), short_field_name.c_str());
                return;
            }

            printf("%sclass_wrapper.add_member(\"%s\", &%s);\n", indentation.c_str(),
                   short_field_name.c_str(), full_field_name.c_str());
//            printf("%sData member %s, type: %s\n",
//                   indentation.c_str(),
//                   field->getNameAsString().c_str(),
//                   field->getType().getAsString().c_str());

        }

        std::string get_method_parameters(CXXMethodDecl * method) {
            std::stringstream result;
            bool first_param = true;
            for (unsigned int i = 0; i < method->getNumParams(); i++) {
                if (!first_param) {
                    result << ", ";
                }
                first_param = false;
                auto param = method->getParamDecl(i);
                result << param->getOriginalType().getAsString();
            }
            return result.str();
        }

        void handle_method(CXXMethodDecl * method, EXPORT_TYPE parent_export_type, const std::string & indentation) {

            std::string full_method_name(method->getQualifiedNameAsString());
            std::string short_method_name(method->getNameAsString());
            auto export_type = get_export_type(method, parent_export_type);

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping method %s because not supposed to be exported %d\n",
                       indentation.c_str(), full_method_name.c_str(), export_type);
                return;
            }

            // only deal with public methods
            if (method->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), full_method_name.c_str());
                return;
            }
            if (method->isOverloadedOperator()) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping overloaded operator %s\n", indentation.c_str(), full_method_name.c_str());
                return;
            }
            if (dyn_cast<CXXConstructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping constructor %s\n", indentation.c_str(), full_method_name.c_str());
                return;
            }
            if (dyn_cast<CXXDestructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping destructor %s\n", indentation.c_str(), full_method_name.c_str());
                return;
            }
            if (method->isPure()) {
                assert(method->isVirtual());
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), full_method_name.c_str());
                return;
            }

            printf("%s", indentation.c_str());


            if (method->isStatic()) {
                printf("class_wrapper.add_static_method<%s>(\"%s\", &%s);\n",
                       get_method_parameters(method).c_str(),
                       short_method_name.c_str(), full_method_name.c_str());
            } else {
                printf("class_wrapper.add_method<%s>(\"%s\", &%s);\n",
                       get_method_parameters(method).c_str(),
                       short_method_name.c_str(), full_method_name.c_str());

            }
        }


        void handle_class(const CXXRecordDecl * klass,
                          EXPORT_TYPE parent_export_type = EXPORT_UNSPECIFIED,
                          bool top_level = true,
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
//            printf("%sClass/struct: %s\n", indentation.c_str(), class_name.c_str());
            if (top_level) printf("{\n");
            if (top_level) printf("v8toolkit::V8ClassWrapper<%s> class_wrapper = isolate.wrap_class<%s>(isolate);\n",
                class_name.c_str(), class_name.c_str());

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
                handle_class(record_decl, export_type, false, indentation + "  ");
            }
            if (top_level) printf("}\n");
        }


        virtual void run(const MatchFinder::MatchResult &Result) {
            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class")) {
                handle_class(klass);

                BidirectionalBindings bidirectional(klass);
                bidirectional.print_bindings();
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
        X("v8toolkit-generate-bindings", "generate v8toolkit bindings");