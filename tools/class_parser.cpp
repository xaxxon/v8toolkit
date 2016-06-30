
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
#include <fstream>
#include <sstream>
#include <regex>

#include <cppformat/format.h>


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

#include "class_parser.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;

using namespace std;

//#define PRINT_SKIPPED_EXPORT_REASONS true
#define PRINT_SKIPPED_EXPORT_REASONS false



namespace {

    std::string get_method_parameters(const CXXMethodDecl * method) {
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

    std::string get_method_string(const CXXMethodDecl * method) {
        std::stringstream result;
        result << method->getReturnType().getAsString();

        result << method->getName().str();

        result << "(";

        result << get_method_parameters(method);

        result << ")";

        return result.str();
    }



    // calls callback for each constructor in the class
    template<class Callback>
    void foreach_constructor(const CXXRecordDecl * klass, Callback && callback) {
        for(CXXMethodDecl * method : klass->methods()) {
            CXXConstructorDecl * constructor;
            if ((constructor = dyn_cast<CXXConstructorDecl>(method))) {
                callback(constructor);
            }
        }
    }

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

    std::vector<std::string> get_annotation_regex(const Decl * decl, const std::string & regex_string) {
        auto regex = std::regex(regex_string);
        std::vector<std::string> results;

        auto annotations = get_annotations(decl);
        for (auto & annotation : annotations) {
            std::smatch matches;
            if (std::regex_match(annotation, matches, regex)) {
//                printf("GOT %d MATCHES\n", (int)matches.size());
                if (matches.size() > 1) {
                    results.push_back(matches[1]);
                }
            }
        }
        return results;
    }



    class BidirectionalBindings {
    private:
        const CXXRecordDecl * starting_class;

    public:
        BidirectionalBindings(const CXXRecordDecl * starting_class) : starting_class(starting_class){}
        std::string short_name(){return starting_class->getName();}



        std::vector<const CXXMethodDecl *> get_all_virtual_methods_for_class(const CXXRecordDecl * klass) {
            std::vector<const CXXMethodDecl *> results;
            std::deque<const CXXRecordDecl *> stack{klass};

            while (!stack.empty()) {
                auto current_class = stack.front();
                stack.pop_front();

                for(CXXMethodDecl * method : current_class->methods()) {
                    if (method->isVirtual() && !method->isPure()) {
                        // go through existing ones and check for match
                        if (std::find_if(results.begin(), results.end(), [&](auto found){
                            if(get_method_string(method) == get_method_string(found)) {
//                                printf("Found dupe: %s\n", get_method_string(method).c_str());
                                return true;
                            }
                            return false;
                        }) == results.end()) {
                            results.push_back(method);
                        }
                    }
                }

                for (auto base_class : current_class->bases()) {
                    auto base_decl = base_class.getType()->getAsCXXRecordDecl();
                    stack.push_back(base_decl);
                }
            }
            return results;
        }

        std::string handle_virtual(const CXXMethodDecl * method) {

            // skip pure virtual functions
            if (method->isPure()) {
                return "";
            }
//            printf("Dealing with %s\n", method->getQualifiedNameAsString().c_str());
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
            result << method_name.str() << ");\n";

            return result.str();

        }

        std::string handle_class(const CXXRecordDecl * klass) {
            std::stringstream result;
            auto virtuals = get_all_virtual_methods_for_class(klass);
            for (auto method : virtuals) {
                result << handle_virtual(method);
            }
            return result.str();

        }

        std::string get_bindings() {
            std::stringstream result;
            auto annotations = get_annotations(starting_class);
            auto matches = get_annotation_regex(starting_class, "v8toolkit_generate_(.*)");
            if (has_annotation(starting_class, "v8toolkit_generate_bidirectional")) {
                result << fmt::format("class JS{} : public {}, public v8toolkit::JSWrapper<{}> {{\n", // {{ is escaped {
                                      short_name(), short_name(), short_name());
                result << handle_class(starting_class);
                result << "};\n";
            } else {
//                printf("Class %s not marked bidirectional\n", short_name().c_str());
            }

            return result.str();
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
        ofstream & class_wrapper_file;
        ofstream & bidirectional_class_file;

    public:

        EXPORT_TYPE get_export_type(const Decl * decl, EXPORT_TYPE previous = EXPORT_UNSPECIFIED) {
            auto &attrs = decl->getAttrs();
            EXPORT_TYPE export_type = previous;

            for (auto attr : attrs) {
                if (dyn_cast<AnnotateAttr>(attr)) {
                    auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                    auto annotation_string = attribute_attr->getAnnotation().str();



                    if (annotation_string == V8TOOLKIT_ALL_STRING) {
                        export_type = EXPORT_ALL;
                    } else if (annotation_string == "v8toolkit_generate_bindings_some") {
                        export_type = EXPORT_SOME;
                    } else if (annotation_string == "v8toolkit_generate_bindings_except") {
                        export_type = EXPORT_EXCEPT;
                    } else if (annotation_string == V8TOOLKIT_NONE_STRING) {
                        export_type = EXPORT_NONE; // just for completeness
                    }
                }
            }
//            printf("Returning export type: %d\n", export_type);
            return export_type;
        }


        ClassHandler(CompilerInstance &CI,
                     ofstream & class_wrapper_file,
                     ofstream & bidirectional_class_file) :
            source_manager(CI.getSourceManager()),
            class_wrapper_file(class_wrapper_file),
            bidirectional_class_file(bidirectional_class_file)
        {}



        std::string handle_data_member(FieldDecl * field, EXPORT_TYPE parent_export_type, const std::string & indentation) {
            std::stringstream result;
            auto export_type = get_export_type(field, parent_export_type);
            auto short_field_name = field->getNameAsString();
            auto full_field_name = field->getQualifiedNameAsString();


            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping data member %s because not supposed to be exported %d\n",
                       indentation.c_str(),
                       short_field_name.c_str(), export_type);
                return "";
            }

            if (field->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), short_field_name.c_str());
                return "";
            }

            result << fmt::format("{}class_wrapper.add_member(\"{}\", &{});\n", indentation,
                   short_field_name, full_field_name);
//            printf("%sData member %s, type: %s\n",
//                   indentation.c_str(),
//                   field->getNameAsString().c_str(),
//                   field->getType().getAsString().c_str());
            return result.str();
        }


        std::string handle_method(CXXMethodDecl * method, EXPORT_TYPE parent_export_type, const std::string & indentation) {

            std::stringstream result;

            std::string full_method_name(method->getQualifiedNameAsString());
            std::string short_method_name(method->getNameAsString());
            auto export_type = get_export_type(method, parent_export_type);

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping method %s because not supposed to be exported %d\n",
                       indentation.c_str(), full_method_name.c_str(), export_type);
                return "";
            }

            // only deal with public methods
            if (method->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**%s is not public, skipping\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }
            if (method->isOverloadedOperator()) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping overloaded operator %s\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }
            if (dyn_cast<CXXConstructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping constructor %s\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }
            if (dyn_cast<CXXDestructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping destructor %s\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }
            if (method->isPure()) {
                assert(method->isVirtual());
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }

            result << indentation;


            if (method->isStatic()) {
                result << fmt::format("class_wrapper.add_static_method<{}>(\"{}\", &{});\n",
                       get_method_parameters(method),
                       short_method_name, full_method_name);
            } else {
                result << fmt::format("class_wrapper.add_method<{}>(\"{}\", &{});\n",
                       get_method_parameters(method),
                       short_method_name, full_method_name);

            }
            return result.str();
        }


        std::string handle_class(const CXXRecordDecl * klass,
                          EXPORT_TYPE parent_export_type = EXPORT_UNSPECIFIED,
                          bool top_level = true,
                          const std::string & indentation = "") {

            std::stringstream result;

            bool is_bidirectional = false;
            if (top_level) {
                if (has_annotation(klass, V8TOOLKIT_BIDIRECTIONAL_STRING)) {
//                    printf("Is bidirectional\n");
                    is_bidirectional = true;
                } else {
//                    printf("Is *not* bidirectional\n");
                }
            }

            auto class_name = klass->getQualifiedNameAsString();
            auto export_type = get_export_type(klass, parent_export_type);
            if (export_type == EXPORT_NONE) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%sSkipping class %s marked EXPORT_NONE\n", indentation.c_str(), class_name.c_str());
                return "";
            }

            // prints out source for decl
            //printf("class at %s", decl2str(klass,  source_manager).c_str());

//            auto full_source_loc = FullSourceLoc(klass->getLocation(), source_manager);
//            printf("%sClass/struct: %s\n", indentation.c_str(), class_name.c_str());
            if (top_level) result << "{\n";
            if (top_level) result << fmt::format("  v8toolkit::V8ClassWrapper<{}> class_wrapper = isolate.wrap_class<{}>(isolate);\n",
                class_name, class_name);

//            printf("%s Decl at line %d, file id: %d\n", indentation.c_str(), full_source_loc.getExpansionLineNumber(),
//                   full_source_loc.getFileID().getHashValue());

//                auto type_decl = dyn_cast<TypeDecl>(klass);
//                assert(type_decl);
//                auto type = type_decl->getTypeForDecl();
//
            for(CXXMethodDecl * method : klass->methods()) {
                result << handle_method(method, export_type, indentation + "  ");
            }

            for (FieldDecl * field : klass->fields()) {
                result << handle_data_member(field, export_type, indentation + "  ");
            }


            for (auto base_class : klass->bases()) {
                auto qual_type = base_class.getType();
                auto record_decl = qual_type->getAsCXXRecordDecl();
                handle_class(record_decl, export_type, false, indentation + "  ");
            }

            if (is_bidirectional) {
                result << fmt::format("{}  JSFactory<{}, JS{}>::add_subclass_static_method(class_wrapper);\n",
                                         indentation,
                                         class_name, class_name);
            }

            if (top_level) result << fmt::format("{}  class_wrapper.finalize();\n", indentation);

            std::vector<std::string> used_constructor_names;
            foreach_constructor(klass, [&](const CXXMethodDecl * constructor) {
                auto annotations = get_annotation_regex(constructor, V8TOOLKIT_CONSTRUCTOR_PREFIX "(.*)");
//                printf("Got %d annotations on constructor\n", (int)annotations.size());
                std::string constructor_name = class_name;
                if (!annotations.empty()) {
                    constructor_name = annotations[0];
                }
                if (std::find(used_constructor_names.begin(), used_constructor_names.end(), constructor_name) != used_constructor_names.end()) {
                    cerr << fmt::format("Error: because duplicate JS constructor function name {}", constructor_name.c_str()) << endl;
                    throw std::exception();
                }
                used_constructor_names.push_back(constructor_name);

                result << fmt::format("{}  class_wrapper.add_constructor<{}>(\"{}\", isolate);\n",
                       indentation, get_method_parameters(constructor), constructor_name);
            });

            if (top_level) result << "}\n\n";
            return result.str();
        }


        /**
         * This runs per-match
         */
        virtual void run(const MatchFinder::MatchResult &Result) {

            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class")) {
                auto full_class_name = klass->getQualifiedNameAsString();

                auto class_wrapper_code = handle_class(klass);
                class_wrapper_file << class_wrapper_code;


                BidirectionalBindings bidirectional(klass);
                auto bidirectional_class_code = bidirectional.get_bindings();
                bidirectional_class_file << bidirectional_class_code;
            }
        }
    };


    // Implementation of the ASTConsumer interface for reading an AST produced
    // by the Clang parser. It registers a couple of matchers and runs them on
    // the AST.
    class MyASTConsumer : public ASTConsumer {
    public:
        MyASTConsumer(CompilerInstance &CI,
                      ofstream & class_wrapper_file,
                      ofstream & bidirectional_class_file) :
                HandlerForClass(CI, class_wrapper_file, bidirectional_class_file) {
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
    public:
        // open up output files
        PrintFunctionNamesAction() {
            const char * class_wrapper_filename = "v8toolkit_generated_class_wrapper.cpp";
            class_wrapper_file.open(class_wrapper_filename, ios::out);
            if (!class_wrapper_file) {
                cerr << "Couldn't open " << class_wrapper_filename << endl;
                throw std::exception();
            }
            class_wrapper_file << "void v8toolkit_initialize_class_wrappers() {" << endl;

            const char * bidirectional_class_filename = "v8toolkit_generated_bidirectional_class.h";
            bidirectional_class_file.open(bidirectional_class_filename, ios::out);
            if (!bidirectional_class_file) {
                cerr << "Couldn't open " << bidirectional_class_filename << endl;
                throw std::exception();
            }
        }

        void EndSourceFileAction() {
            static bool already_called = false;

            if (already_called) {
                cerr << "This plugin doesn't work if there's more than one file.   Use it on a unity build" << endl;
                throw std::exception();
            }
            already_called = true;
            // close up c++ syntax and close output files
            class_wrapper_file << "}\n";
            class_wrapper_file.close();

            bidirectional_class_file.close();

        }

    protected:
        ofstream class_wrapper_file;
        ofstream bidirectional_class_file;


        // The value returned here is used internally to run checks against
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                       llvm::StringRef) {

            return llvm::make_unique<MyASTConsumer>(CI, class_wrapper_file, bidirectional_class_file);
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