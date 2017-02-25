/**

FOR COMMENT PARSING, LOOK AT ASTContext::getCommentForDecl

http://clang.llvm.org/doxygen/classclang_1_1ASTContext.html#aa3ec454ca3698f73c421b08f6edcea92

 */

/**
 * This clang plugin looks for classes annotated with V8TOOLKIT_WRAPPED_CLASS and/or V8TOOLKIT_BIDIRECTIONAL_CLASS
 * and automatically generates source files for class wrappings and/or JSWrapper objects for those classes.
 *
 * Each JSWrapper object type will go into its own .h file called v8toolkit_generated_bidirectional_<ClassName>.h
 *   These files should be included from within the header file defining the class.
 *
 * MISSING DOCS FOR CLASS WRAPPER CODE GENERATION
 */

// This program will only work with clang but the output should be useable on any platform.

/**
 * KNOWN BUGS:
 * Doesn't properly understand virtual methods and will duplicate them across the inheritence hierarchy
 * Doesn't include root file of compilation - if this is pretty safe in a unity build, as the root file is a file that
 *   just includes all the other files
 */


/**
How to run over complete code base using cmake + cotiregg1

add_library(api-gen-template OBJECT ${YOUR_SOURCE_FILES})
target_compile_options(api-gen-template
        PRIVATE -Xclang -ast-dump -fdiagnostics-color=never <== this isn't right yet, this just dumps the ast
        )
set_target_properties(api-gen-template PROPERTIES COTIRE_UNITY_TARGET_NAME "api-gen")
cotire(api-gen-template)
 */

#include <vector>
#include <string>
#include <map>
using namespace std;


//////////////////////////////
// CUSTOMIZE THESE VARIABLES
//////////////////////////////

// if this is defined, only template info will be printed
//#define TEMPLATE_INFO_ONLY
#define TEMPLATE_FILTER_STD

#define TEMPLATED_CLASS_PRINT_THRESHOLD 10
#define TEMPLATED_FUNCTION_PRINT_THRESHOLD 100


// Generate an additional file with sfinae for each wrapped class type
bool generate_v8classwrapper_sfinae = true;

// Having this too high can lead to VERY memory-intensive compilation units
// Single classes (+base classes) with more than this number of declarations will still be in one file.
// TODO: This should be a command line parameter to the plugin
#define MAX_DECLARATIONS_PER_FILE 40

// Any base types you want to always ignore -- v8toolkit::WrappedClassBase must remain, others may be added/changed
vector<string> base_types_to_ignore = {"class v8toolkit::WrappedClassBase", "class Subscriber"};


// Top level types that will be immediately discarded
vector<string> types_to_ignore_regex = {"^struct has_custom_process[<].*[>]::mixin$"};

vector<string> includes_for_every_class_wrapper_file = {"<stdbool.h>", "\"js_casts.h\"", "<v8toolkit/v8_class_wrapper_impl.h>"};

// error if bidirectional types don't make it in due to include file ordering
// disable "fast_compile" so the V8ClassWrapper code can be generated 
string header_for_every_class_wrapper_file = "#define NEED_BIDIRECTIONAL_TYPES\n#undef V8TOOLKIT_WRAPPER_FAST_COMPILE\n";

// sometimes files sneak in that just shouldn't be
vector<string> never_include_for_any_file = {"\"v8helpers.h\""};



map<string, string> static_method_renames = {{"name", "get_name"}};

// http://usejsdoc.org/tags-type.html
map<string, string> cpp_to_js_type_conversions = {{"^(?:std::)?vector[<]\\s*([^>]+?)\\s*[>]\\s*$", "Array.{$1}"},
                                                  {"^(?:std::)?map[<]\\s*([^>]+?)\\s*,\\s*([^>]+?)\\s*[>]\\s*$", "Object.{$1, $2}"},
                                                  {"^([^<]+)\\s*[<]\\s*(.+?)\\s*[>]\\s*([^>]*)$", "$1<$2>$3"},
                                                  {"^(?:const)?\\s*(?:unsigned)?\\s*(?:char|short|int|long|long long|float|double|long double)\\s*(?:const)?\\s*[*]?\\s*[&]?$", "Number"},
                                                  {"^(?:const)?\\s*_?[Bb]ool\\s*(?:const)?\\s*[*]?\\s*[&]?$", "Boolean"},
                                                  {"^(?:const)?\\s*(?:char\\s*[*]|(?:std::)?string)\\s*(?:const)?\\s*\\s*[&]?$", "String"},
                                                  {"^void$", "Undefined"}};

// regex for @callback instead of @param: ^(const)?\s*(std::)?function[<][^>]*[>]\s*(const)?\s*\s*[&]?$

std::string js_api_header = R"JS_API_HEADER(
/**
 * Prints a string and appends a newline
 * @param {string} s the string to be printed
 */
function println(s){}

/**
 * Prints a string without adding a newline to the end
 * @param {string} s the string to be printed
 */
function print(s){}

/**
 * Dumps the contents of the given variable - only 'own' properties
 * @param o the variable to be dumped
 */
function printobj(o)

/**
 * Dumps the contents of the given variable - all properties including those of prototype chain
 * @param o the variable to be dumped
 */
function printobjall(o)

/**
 * Attempts to load the given module and returns the exported data.  Requiring the same module
 *   more than once will return the cached result, not re-execute the source.
 * @param {string} module_name name of the module to require
 */
function require(module_name) {}


)JS_API_HEADER";

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <regex>

#include <fmt/ostream.h>


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

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
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/Comment.h"
#include "llvm/Support/raw_ostream.h"

#pragma clang diagnostic pop

#include "class_parser.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::driver;
using namespace clang::tooling;
using namespace clang::comments;
using namespace std;



#define PRINT_SKIPPED_EXPORT_REASONS true
//#define PRINT_SKIPPED_EXPORT_REASONS false

int classes_wrapped = 0;
int methods_wrapped = 0;
int matched_classes_returned = 0;

namespace {

    std::string get_canonical_name_for_decl(const TypeDecl * decl) {
        if (decl == nullptr) {
            llvm::report_fatal_error("null type decl to get_canonical_name_for_decl");
        }
        return decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString();
    }

//
//    std::string get_full_name_for_type(QualType qual_type) {
//
//        if (auto typedef_decl = qual_type->getAs<TypedefNameDecl>()) {
//            qual_type = typedef_decl->getUnderlyingType();
//        }
//        auto record_decl = qual_type->getAsCXXRecordDecl();
//        if (record_decl == nullptr) {
//            std::cerr << fmt::format("Couldn't get record decl for {}", qual_type.getAsString()) << std::endl;
//            assert(record_decl != nullptr);
//        }
//
//        return get_canonical_name_for_decl(record_decl);
//    }



        int print_logging = 1;

    // how was a wrapped class determined to be a wrapped class?
    enum FOUND_METHOD {
        FOUND_UNSPECIFIED = 0,
        FOUND_ANNOTATION,
        FOUND_INHERITANCE,
        FOUND_GENERATED,
        FOUND_BASE_CLASS, // if the class is a base class of a wrapped type, the class must be wrapped
        FOUND_NEVER_WRAP
    };


    enum EXPORT_TYPE {
        EXPORT_UNSPECIFIED = 0,
        EXPORT_NONE, // export nothing
        EXPORT_SOME, // only exports specifically marked entities
        EXPORT_EXCEPT, // exports everything except specifically marked entities
        EXPORT_ALL}; // exports everything

    EXPORT_TYPE get_export_type(const NamedDecl * decl, EXPORT_TYPE previous = EXPORT_UNSPECIFIED);



    // list of data (source code being processed) errors occuring during the run.   Clang API errors are still immediate fails with
    //   llvm::report_fatal_error
    vector<string> errors;
    void data_error(const string & error) {
        cerr << "DATA ERROR: " << error << endl;
        errors.push_back(error);
    }

    vector<string> warnings;
    void data_warning(const string & warning) {
	cerr << "DATA WARNING: " << warning << endl;
	warnings.push_back(warning);
    }

    QualType get_plain_type(QualType qual_type) {
	auto type = qual_type.getNonReferenceType().getUnqualifiedType();
	while(!type->getPointeeType().isNull()) {
	    type = type->getPointeeType().getUnqualifiedType();
	}
	return type;
    }

    
    class PrintLoggingGuard {
        bool logging = false;
    public:
        PrintLoggingGuard() = default;
        ~PrintLoggingGuard() {
            if (logging) {
                print_logging--;
            }
        }
        void log(){
            print_logging++;
            logging = true;
        }
    };

    /*
    std::string handle_std(const std::string & input) {
        smatch matches;
	string result = input;
	// EricWF said to remove __[0-9] just to be safe for future updates
        if (regex_match(input, matches, regex("^((?:const\\s+|volatile\\s+)*)(?:class |struct )?(?:std::(?:__[0-9]::)?)?(.*)"))) {
	    // space before std:: is handled from const/volatile if needed
	    result = matches[1].str() + "std::" + matches[2].str();

	}

	if (print_logging) cerr << "Stripping std from " << input << " results in " << result << endl;
        return result;
    }


    bool has_std(const std::string & input) {
        return std::regex_match(input, regex("^(const\\s+|volatile\\s+)*(class |struct )?\\s*std::.*$"));
    }
    */


    void print_vector(const vector<string> & vec, const string & header = "", const string & indentation = "", bool ignore_empty = true) {

        if (ignore_empty && vec.empty()) {
            return;
        }

        if (header != "") {
            cerr << indentation << header << endl;
        }
        for (auto & str : vec) {
            cerr << indentation << " - " << str << endl;
        }
        if (vec.empty()) {
            cerr << indentation << " * (EMPTY VECTOR)" << endl;
        }
    }

    // joins a range of strings with commas (or whatever specified)
    template<class T>
    std::string join(const T & source, const std::string & between = ", ", bool leading_between = false) {
        if (source.empty()) {
            return "";
        }
        stringstream result;
        if (leading_between) {
            result << between;
        }
        bool first = true;
        for (auto & str : source) {
            if (str == "") {
                //printf("Skipping blank entry in join()\n");
                continue;
            }
            if (!first) { result << between;}
            first = false;
            result << str;
        }
        return result.str();
    }

    /* For some classes (ones with dependent base types among them), there will be two copies, one will be
     * unusable for an unknown reason.  This tests if it is that
     */
    bool is_good_record_decl(const CXXRecordDecl * decl) {
        if (decl == nullptr) {
            return false;
        }

        if (!decl->isThisDeclarationADefinition()) {
            return true;
        }

        for (auto base : decl->bases()) {
            if (base.getType().getTypePtrOrNull() == nullptr) {
                llvm::report_fatal_error(fmt::format("base type ptr was null for {}", decl->getNameAsString()).c_str());
            }
            if (!is_good_record_decl(base.getType()->getAsCXXRecordDecl())) {
                return false;
            }
        }
        return true;
    }


    // Finds where file_id is included, how it was included, and returns the string to duplicate
    //   that inclusion
    std::string get_include_string_for_fileid(CompilerInstance & compiler_instance, FileID & file_id) {
        auto include_source_location = compiler_instance.getPreprocessor().getSourceManager().getIncludeLoc(file_id);

        // If it's in the "root" file (file id 1), then there's no include for it
        if (include_source_location.isValid()) {
            auto header_file = include_source_location.printToString(compiler_instance.getPreprocessor().getSourceManager());
//            if (print_logging) cerr << "include source location: " << header_file << endl;
            //            wrapped_class.include_files.insert(header_file);
        } else {
//            if (print_logging) cerr << "No valid source location" << endl;
            return "";
        }

        bool invalid;
        // This gets EVERYTHING after the start of the filename in the include.  "asdf.h"..... or <asdf.h>.....
        const char * text = compiler_instance.getPreprocessor().getSourceManager().getCharacterData(include_source_location, &invalid);
        const char * text_end = text + 1;
        while(*text_end != '>' && *text_end != '"') {
            text_end++;
        }

        return string(text, (text_end - text) + 1);

    }


    std::string get_include_for_source_location(CompilerInstance & compiler_instance, const SourceLocation & source_location) {
        auto full_source_loc = FullSourceLoc(source_location, compiler_instance.getPreprocessor().getSourceManager());

        auto file_id = full_source_loc.getFileID();
        return get_include_string_for_fileid(compiler_instance, file_id);
    }

    std::string get_include_for_type_decl(CompilerInstance & compiler_instance, const TypeDecl * type_decl) {
        if (type_decl == nullptr) {
            return "";
        }
        return get_include_for_source_location(compiler_instance, type_decl->getLocStart());
    }


//
//    std::string decl2str(const clang::Decl *d, SourceManager &sm) {
//        // (T, U) => "T,,"
//        std::string text = Lexer::getSourceText(CharSourceRange::getTokenRange(d->getSourceRange()), sm, LangOptions(), 0);
//        if (text.at(text.size()-1) == ',')
//            return Lexer::getSourceText(CharSourceRange::getCharRange(d->getSourceRange()), sm, LangOptions(), 0);
//        return text;
//    }


    std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range) {
        std::string text = Lexer::getSourceText(CharSourceRange::getTokenRange(source_range), sm, LangOptions(), 0);
        if (!text.empty() && text.at(text.size()-1) == ',')
            return Lexer::getSourceText(CharSourceRange::getCharRange(source_range), sm, LangOptions(), 0);
        return text;
    }
#if 0

    vector<string> count_top_level_template_parameters(const std::string & source) {
        int open_angle_count = 0;
        vector<string> parameter_strings;
        std::string * current;
        for (char c : source) {
            if (isspace(c)) {
                continue;
            }
            if (c == '<') {
                open_angle_count++;
                if (open_angle_count > 1) {
                    *current += c;
                }
            } else if (c == '>') {
                open_angle_count--;
                if (open_angle_count > 0) {
                    *current += c;
                }
            } else {
                if (open_angle_count == 1) {
                    if (parameter_strings.size() == 0) {
                        parameter_strings.push_back("");
                        current = &parameter_strings.back();
                    }
                    if (c == ',') {
                        parameter_strings.push_back("");
                        current = &parameter_strings.back();
                        if (open_angle_count > 1) {
                            *current += c;
                        }
                    } else {
                        *current += c;
                    }
                } else if (open_angle_count > 1) {
                    *current += c;
                }
            }
        }
        if (print_logging) if (print_logging) cerr << "^^^^^^^^^^^^^^^ Counted " << parameter_strings.size() << " for " << source << endl;
        for (auto & str : parameter_strings) {
            if (print_logging) cerr <<  "^^^^^^^^^^^^^^^" << str << endl;
        }
        return parameter_strings;
    }
#endif

#include "annotations.h"
    map<const ClassTemplateDecl *, Annotations> Annotations::annotations_for_class_templates;

    // any annotations on 'using' statements should be applied to the actual CXXRecordDecl being aliased (the right side)
    map<const CXXRecordDecl *, Annotations> Annotations::annotations_for_record_decls;


    // if a template instantiation is named with a 'using' statement, use that alias for the type isntead of the template/class name itself
    //   this stops them all from being named the same thing - aka CppFactory, CppFactory, ...  instead of MyThingFactory, MyOtherThingFactory, ...
    map<const CXXRecordDecl *, string> Annotations::names_for_record_decls;




    struct ClassTemplate;
    vector<std::unique_ptr<ClassTemplate>> class_templates;
    struct ClassTemplate {
        std::string name;
        const ClassTemplateDecl * decl;
        int instantiations = 0;

        ClassTemplate(const ClassTemplateDecl * decl) : decl(decl) {
            name = decl->getQualifiedNameAsString();
            // cerr << fmt::format("Created class template for {}", name) << endl;
        }

        void instantiated(){ instantiations++; }



        static ClassTemplate & get_or_create(const ClassTemplateDecl * decl) {
            for(auto & tmpl : class_templates) {
                if (tmpl->decl == decl) {
                    return *tmpl;
                }
            }
            class_templates.emplace_back(make_unique<ClassTemplate>(decl));
            return *class_templates.back();
        }
    };

    struct FunctionTemplate;
    vector<unique_ptr<FunctionTemplate>> function_templates;
    struct FunctionTemplate {
        std::string name;
        //const FunctionTemplateDecl * decl;

        // not all functions instantiated because of a template are templated themselves
        const FunctionDecl * decl;
        int instantiations = 0;

        FunctionTemplate(const FunctionDecl * decl) : decl(decl) {
            name = decl->getQualifiedNameAsString();
            //	    cerr << fmt::format("Created function template for {}", name) << endl;
        }

        void instantiated(){ instantiations++; }


        static FunctionTemplate & get_or_create(const FunctionDecl * decl) {

            for(auto & tmpl : function_templates) {
                if (tmpl->decl == decl) {
                    return *tmpl;
                }
            }
            function_templates.emplace_back(make_unique<FunctionTemplate>(decl));
            return *function_templates.back();
        }
    };

    // store all the constructor names used, since they all go into the same global object template used as for building contexts
    std::vector<std::string> used_constructor_names;

#include "parsed_method.h"
#include "wrapped_class.h"

    std::vector<WrappedClass *> definitions_to_process;

    void add_definition(WrappedClass & wrapped_class) {
        if (!wrapped_class.decl->isThisDeclarationADefinition()) {
            llvm::report_fatal_error(fmt::format("tried to add non-definition to definition list for post processing: {}", wrapped_class.class_name).c_str());
        }
        if (std::find(definitions_to_process.begin(), definitions_to_process.end(), &wrapped_class) == definitions_to_process.end()) {
            definitions_to_process.push_back(&wrapped_class);
        }
    }




    std::vector<unique_ptr<WrappedClass>> wrapped_classes;

    /*
    vector<WrappedClass *> get_wrapped_class_regex(const string & regex_string) {
	cerr << "Searching with regex: " << regex_string << endl;
	vector<WrappedClass *> results;
	for (auto & wrapped_class : wrapped_classes) {
	    cerr << " -- " << wrapped_class->class_name << endl;
	    if (regex_search(wrapped_class->class_name, regex(regex_string))) {
		cerr << " -- ** MATCH ** " << endl;
		results.push_back(wrapped_class.get());
	    }
	}
	cerr << fmt::format("Returning {} results", results.size()) << endl;
	return results;
    }
    */

    bool has_wrapped_class(const CXXRecordDecl * decl) {
        auto class_name = get_canonical_name_for_decl(decl);

        for (auto & wrapped_class : wrapped_classes) {

            if (wrapped_class->class_name == class_name) {
                return true;
            }
        }
        return false;
    }

    WrappedClass & get_or_insert_wrapped_class(const CXXRecordDecl * decl,
                                               CompilerInstance & compiler_instance,
                                               FOUND_METHOD found_method) {

        if (decl->isDependentType()) {
            throw exception();
        }

        auto class_name = get_canonical_name_for_decl(decl);

        if (!decl->isThisDeclarationADefinition()) {

            cerr << class_name << " is not a definition - getting definition..." << endl;
            if (!decl->hasDefinition()) {

                llvm::report_fatal_error(fmt::format("{} doesn't have a definition", class_name).c_str());
            }

            decl = decl->getDefinition();
        }


        //fprintf(stderr, "get or insert wrapped class %p\n", (void*)decl);
        //fprintf(stderr, " -- class name %s\n", class_name.c_str());
        for (auto & wrapped_class : wrapped_classes) {

            if (wrapped_class->class_name == class_name) {

                // promote found_method if FOUND_BASE_CLASS is specified - the type must ALWAYS be wrapped
                //   if it is the base of a wrapped type
                if (found_method == FOUND_BASE_CLASS) {

                    // if the class wouldn't otherwise be wrapped, need to make sure no constructors are created
                    if (!wrapped_class->should_be_wrapped()) {
                        wrapped_class->force_no_constructors = true;
                    }
                    wrapped_class->found_method = FOUND_BASE_CLASS;
                }
                //fprintf(stderr, "returning existing object: %p\n", (void *)wrapped_class.get());
                return *wrapped_class;
            }
        }

        auto up = std::make_unique<WrappedClass>(decl, compiler_instance, found_method);

        wrapped_classes.emplace_back(std::move(up));

        //fprintf(stderr, "get or insert wrapped class returning new object: %p\n", (void*)wrapped_classes.back().get());
        return *wrapped_classes.back();
    }




    string get_sfinae_matching_wrapped_classes(const vector<unique_ptr<WrappedClass>> & wrapped_classes) {
        vector<string> sfinaes;
        string forward_declarations = "#define V8TOOLKIT_V8CLASSWRAPPER_FORWARD_DECLARATIONS ";
        for (auto & wrapped_class : wrapped_classes) {
            if (wrapped_class->found_method == FOUND_INHERITANCE) {
                continue;
            }
            if (!wrapped_class->should_be_wrapped()) {
                continue;
            }
            sfinaes.emplace_back(wrapped_class->make_sfinae_to_match_wrapped_class());
            forward_declarations += wrapped_class->class_name + "; ";
        }


        for(int i = sfinaes.size() - 1; i >= 0; i--) {
            if (sfinaes[i] == "") {
                sfinaes.erase(sfinaes.begin() + i);
            }
        }


        // too many forward declarations do bad things to compile time / ram usage, so try to catch any silly mistakes
        if (sfinaes.size() > 40 /* 40 is arbitrary */) {
            cerr << join(sfinaes, " || ") << endl;
            llvm::report_fatal_error("more 'sfinae's than arbitrary number used to catch likely errors - can be increased if needed");
        }



        std::string sfinae = "";
        if (!sfinaes.empty()) {
            sfinae = string("#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE ") + join(sfinaes, " || ") + "\n";
        } // else if it's empty, leave it undefined

        forward_declarations += "\n";
        return sfinae + "\n" + forward_declarations;
    }








    EXPORT_TYPE get_export_type(const NamedDecl * decl, EXPORT_TYPE previous) {
        auto &attrs = decl->getAttrs();
        EXPORT_TYPE export_type = previous;

        auto name = decl->getNameAsString();

        bool found_export_specifier = false;

        for (auto attr : attrs) {
            if (dyn_cast<AnnotateAttr>(attr)) {
                auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                auto annotation_string = attribute_attr->getAnnotation().str();

                if (annotation_string == V8TOOLKIT_ALL_STRING) {
                    if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name));}

                    export_type = EXPORT_ALL;
                    found_export_specifier = true;
                } else if (annotation_string == "v8toolkit_generate_bindings_some") {
                    if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name));}
                    export_type = EXPORT_SOME;
                    found_export_specifier = true;
                } else if (annotation_string == "v8toolkit_generate_bindings_except") {
                    if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                    export_type = EXPORT_EXCEPT;
                    found_export_specifier = true;
                } else if (annotation_string == V8TOOLKIT_NONE_STRING) {
                    if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                    export_type = EXPORT_NONE; // just for completeness
                    found_export_specifier = true;
                }
            }
        }

        // go through bases looking for specific ones
        if (const CXXRecordDecl * record_decl = dyn_cast<CXXRecordDecl>(decl)) {
            for (auto & base : record_decl->bases()) {
                auto type = base.getType();
                auto base_decl = type->getAsCXXRecordDecl();
                auto base_name = get_canonical_name_for_decl(base_decl);
//                cerr << "%^%^%^%^%^%^%^% " << get_canonical_name_for_decl(base_decl) << endl;
                if (base_name == "class v8toolkit::WrappedClassBase") {
                    cerr << "FOUND WRAPPED CLASS BASE -- EXPORT_ALL" << endl;
                    if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                    export_type = EXPORT_ALL;
                    found_export_specifier = true;
                }
            }
        }

        //        printf("Returning export type: %d for %s\n", export_type, name.c_str());
        return export_type;
    }







//    std::string strip_path_from_filename(const std::string & filename) {
//
//        // naive regex to grab everything after the last slash or backslash
//        auto regex = std::regex("([^/\\\\]*)$");
//
//        std::smatch matches;
//        if (std::regex_search(filename, matches, regex)) {
//            return matches[1];
//        }
//        if (print_logging) cerr << fmt::format("Unrecognizable filename {}", filename);
//        throw std::exception();
//    }

#if 0


    // Returns true if qual_type is a 'trivial' std:: type like
    //   std::string
    bool is_trivial_std_type(QualType & qual_type, std::string & output) {
        std::string name = qual_type.getAsString();
        std::string canonical_name = qual_type.getCanonicalType().getAsString();

        // if it's a std:: type and not explicitly user-specialized, pass it through
        if (std::regex_match(name, regex("^(const\\s+|volatile\\s+)*(class\\s+|struct\\s+)?std::[^<]*$"))) {
            output = handle_std(name);a
            return true;
        }
        // or if the canonical type has std:: in it and it's not user-customized
        else if (has_std(canonical_name) &&
                 std::regex_match(name, regex("^[^<]*$"))) {
            output = handle_std(name);
            return true;
        }
        return false;
    }

    // Returns true if qual_type is a 'non-trivial' std:: type (containing user-specified template types like
    //   std::map<MyType1, MyType2>
    bool is_nontrivial_std_type(QualType & qual_type, std::string & output) {

        std::string name = qual_type.getAsString();
        std::string canonical_name = qual_type.getCanonicalType().getAsString();
        if (print_logging) cerr << "Checking nontrivial std type on " << name << " : " << canonical_name << endl;
        smatch matches;


        // if it's in standard (according to its canonical type) and has user-specified types
        if (has_std(canonical_name) &&
                 std::regex_match(name, matches, regex("^([^<]*<).*$"))) {
            output = handle_std(matches[1].str());
            if (print_logging) cerr << "Yes" << endl;
            return true;
        }
        if (print_logging) cerr << "No" << endl;
        return false;
    }


#endif

    std::string get_type_string(QualType qual_type,
                                const std::string & indentation = "") {

        auto original_qualifiers = qual_type.getLocalFastQualifiers();
        // chase any typedefs to get the "real" type
        while (auto typedef_type = dyn_cast<TypedefType>(qual_type)) {
            qual_type = typedef_type->getDecl()->getUnderlyingType();
        }

        // re-apply qualifiers to potentially new qualtype
        qual_type.setLocalFastQualifiers(original_qualifiers);

        auto canonical_qual_type = qual_type.getCanonicalType();
//        cerr << "canonical qual type typeclass: " << canonical_qual_type->getTypeClass() << endl;
        auto canonical = canonical_qual_type.getAsString();
        return regex_replace(canonical, regex("std::__1::"), "std::");

#if 0
        std::string source = input_source;

        bool turn_logging_off = false;
	/*
        if (regex_match(qual_type.getAsString(), regex("^.*glm.*$") )) {
            print_logging++;
            turn_logging_off = true;
        }
	*/
        
        if (print_logging) cerr << indentation << "Started at " << qual_type.getAsString() << endl;
        if (print_logging) cerr << indentation << "  And canonical name: " << qual_type.getCanonicalType().getAsString() << endl;
        if (print_logging) cerr << indentation << "  And source " << source << endl;

        std::string std_result;
        if (is_trivial_std_type(qual_type, std_result)) {
            if (print_logging) cerr << indentation << "Returning trivial std:: type: " << std_result << endl << endl;
            if (turn_logging_off) print_logging--;
            return std_result;
        }

        bool is_reference = qual_type->isReferenceType();
        string reference_suffix = is_reference ? "&" : "";
        qual_type = qual_type.getNonReferenceType();

        stringstream pointer_suffix;
        bool changed = true;
        while(changed) {
            changed = false;
            if (!qual_type->getPointeeType().isNull()) {
                changed = true;
                pointer_suffix << "*";
                qual_type = qual_type->getPointeeType();
                if (print_logging) cerr << indentation << "stripped pointer, went to: " << qual_type.getAsString() << endl;
                continue; // check for more pointers first
            }

            // This code traverses all the typdefs and pointers to get to the actual base type
            if (dyn_cast<TypedefType>(qual_type) != nullptr) {
                changed = true;
                if (print_logging) cerr << indentation << "stripped typedef, went to: " << qual_type.getAsString() << endl;
                qual_type = dyn_cast<TypedefType>(qual_type)->getDecl()->getUnderlyingType();
                source = ""; // source is invalidated if it's a typedef
            }
        }

        if (print_logging) cerr << indentation << "CHECKING TO SEE IF " << qual_type.getUnqualifiedType().getAsString() << " is a template specialization"<< endl;
        auto base_type_record_decl = qual_type.getUnqualifiedType()->getAsCXXRecordDecl();
        if (dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl)) {



            if (print_logging) cerr << indentation << "!!!!! Started with template specialization: " << qual_type.getAsString() << endl;



            std::smatch matches;
            string qual_type_string = qual_type.getAsString();

            std::string std_type_output;
            bool nontrivial_std_type = false;
            if (is_nontrivial_std_type(qual_type, std_type_output)) {
                if (print_logging) cerr << indentation << "is nontrivial std type and got result: " << std_type_output << endl;
                nontrivial_std_type = true;
                result << std_type_output;
            }
            // Get everything EXCEPT the template parameters into matches[1] and [2]
            else if (!regex_match(qual_type_string, matches, regex("^([^<]+<).*(>[^>]*)$"))) {
                if (print_logging) cerr << indentation << "short circuiting on " << original_qual_type.getAsString() << endl;
                if (turn_logging_off) print_logging--;
                return original_qual_type.getAsString();
            } else {
                result << matches[1];
                if (print_logging) cerr << indentation << "is NOT nontrivial std type" << endl;
            }
            auto template_specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl);

            auto user_specified_template_parameters = count_top_level_template_parameters(source);


            auto & template_arg_list = template_specialization_decl->getTemplateArgs();
            if (user_specified_template_parameters.size() > template_arg_list.size()) {
		llvm::report_fatal_error(fmt::format("ERROR: detected template parameters > actual list size for {}", qual_type_string).c_str());
            }

            auto template_types_to_handle = user_specified_template_parameters.size();
            if (source == "") {
                // if a typedef was followed, then user sources is meaningless
                template_types_to_handle = template_arg_list.size();
            }

//            for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
            for (decltype(template_arg_list.size()) i = 0; i < template_types_to_handle; i++) {
                if (i > 0) {
                    result << ", ";
                }
                if (print_logging) cerr << indentation << "Working on template parameter " << i << endl;
                auto & arg = template_arg_list[i];

                switch(arg.getKind()) {
                    case clang::TemplateArgument::Type: {
                        if (print_logging) cerr << indentation << "processing as type argument" << endl;
                        auto template_arg_qual_type = arg.getAsType();
                        auto template_type_string = get_type_string(template_arg_qual_type,
                                                  indentation + "  ");
                        if (print_logging) cerr << indentation << "About to append " << template_type_string << " template type string onto existing: " << result.str() << endl;
                        result << template_type_string;
                        break; }
                    case clang::TemplateArgument::Integral: {
                        if (print_logging) cerr << indentation << "processing as integral argument" << endl;
                        auto integral_value = arg.getAsIntegral();
                        if (print_logging) cerr << indentation << "integral value radix10: " << integral_value.toString(10) << endl;
                        result << integral_value.toString(10);
                        break;}
                    default:
                        if (print_logging) cerr << indentation << "Oops, unhandled argument type" << endl;
                }
            }
            result << ">" << pointer_suffix.str() << reference_suffix;
            if (print_logging) cerr << indentation << "!!!!!Finished stringifying templated type to: " << result.str() << endl << endl;
            if (turn_logging_off) print_logging--;
            return result.str();

//        } else if (std::regex_match(qual_type.getAsString(), regex("^(class |struct )?std::.*$"))) {
//
//
//            if (print_logging) cerr << indentation << "checking " << qual_type.getAsString();
//            if (dyn_cast<TypedefType>(qual_type)) {
//                if (print_logging) cerr << indentation << " and returning " << dyn_cast<TypedefType>(qual_type)->getDecl()->getQualifiedNameAsString() <<
//                endl << endl;
//                return dyn_cast<TypedefType>(qual_type)->getDecl()->getQualifiedNameAsString() +
//                       (is_reference ? " &" : "");
//            } else {
//                if (print_logging) cerr << indentation << " and returning (no typedef) " << qual_type.getAsString() << endl << endl;
//                return qual_type.getAsString() + pointer_suffix.str() + reference_suffix;
//            }

        } else {

            // THIS APPROACH DOES NOT GENERATE PORTABLE STL NAMES LIKE THE LINE BELOW IS libc++ only not libstdc++
            // std::__1::basic_string<char, struct std::__1::char_traits<char>, class std::__1::allocator<char> >

            // this isn't great because it loses the typedef'd names of things, but it ALWAYS works
            // There is no confusion with reference types or typedefs or const/volatile
            // EXCEPT: it generates a elaborated type specifier which can't be used in certain places
            // http://en.cppreference.com/w/cpp/language/elaborated_type_specifier
            auto canonical_qual_type = original_qual_type.getCanonicalType();

            //printf("Canonical qualtype typedeftype cast: %p\n",(void*) dyn_cast<TypedefType>(canonical_qual_type));

            if (print_logging) cerr << indentation << "returning canonical: " << canonical_qual_type.getAsString() << endl << endl;
            if (turn_logging_off) print_logging--;

            return canonical_qual_type.getAsString();
        }
#endif
    }

    // Gets the "most basic" type in a type.   Strips off ref, pointer, CV
    //   then calls out to get how to include that most basic type definition
    //   and puts it in wrapped_class.include_files
    void update_wrapped_class_for_type(CompilerInstance & compiler_instance,
                                       WrappedClass & wrapped_class,
        // don't capture qualtype by ref since it is changed in this function
                                       QualType qual_type) {

        bool print_logging = true;

        cerr << fmt::format("In update_wrapped_class_for_type {} in wrapped class {}", qual_type.getAsString(), wrapped_class.class_name) << endl;

        if (print_logging) cerr << "Went from " << qual_type.getAsString();
        qual_type = qual_type.getLocalUnqualifiedType();


        // remove pointers
        while(!qual_type->getPointeeType().isNull()) {
            qual_type = qual_type->getPointeeType();
        }
        qual_type = qual_type.getLocalUnqualifiedType();

        if (print_logging) cerr << " to " << qual_type.getAsString() << endl;
        auto base_type_record_decl = qual_type->getAsCXXRecordDecl();



        if (auto function_type = dyn_cast<FunctionType>(&*qual_type)) {
            cerr << "IS A FUNCTION TYPE!!!!" << endl;

            // it feels strange, but the type int(bool) from std::function<int(bool)> is a FunctionProtoType
            if (auto function_prototype = dyn_cast<FunctionProtoType>(function_type)) {
                cerr << "IS A FUNCTION PROTOTYPE" << endl;

                cerr << "Recursing on return type" << endl;
                update_wrapped_class_for_type(compiler_instance, wrapped_class, function_prototype->getReturnType());

                for ( auto param : function_prototype->param_types()) {

                    cerr << "Recursing on param type" << endl;
                    update_wrapped_class_for_type(compiler_instance, wrapped_class, param);
                }
            } else {
                cerr << "IS NOT A FUNCTION PROTOTYPE" << endl;
            }

        } else {
            cerr << "is not a FUNCTION TYPE" << endl;
        }


        // primitive types don't have record decls
        if (base_type_record_decl == nullptr) {
            return;
        }

        auto actual_include_string = get_include_for_type_decl(compiler_instance, base_type_record_decl);

        if (print_logging) cerr << &wrapped_class << "Got include string for " << qual_type.getAsString() << ": " << actual_include_string << endl;

        // if there's no wrapped type, it may be something like a std::function or STL container -- those are ok to not be wrapped
        if (has_wrapped_class(base_type_record_decl)) {
            auto & used_wrapped_class = get_or_insert_wrapped_class(base_type_record_decl, compiler_instance, FOUND_UNSPECIFIED);
            wrapped_class.used_classes.insert(&used_wrapped_class);
        }



        wrapped_class.include_files.insert(actual_include_string);



        if (dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl)) {
            if (print_logging) cerr << "##!#!#!#!# Oh shit, it's a template type " << qual_type.getAsString() << endl;

            auto template_specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl);

            // go through the template args
            auto & template_arg_list = template_specialization_decl->getTemplateArgs();
            for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
                auto & arg = template_arg_list[i];

                // this code only cares about types, so skip non-type template arguments
                if (arg.getKind() != clang::TemplateArgument::Type) {
                    continue;
                }
                auto template_arg_qual_type = arg.getAsType();
                if (template_arg_qual_type.isNull()) {
                    if (print_logging) cerr << "qual type is null" << endl;
                    continue;
                }
                if (print_logging) cerr << "Recursing on templated type " << template_arg_qual_type.getAsString() << endl;
                update_wrapped_class_for_type(compiler_instance, wrapped_class, template_arg_qual_type);
            }
        } else {
            if (print_logging) cerr << "Not a template specializaiton type " << qual_type.getAsString() << endl;
        }
    }


    /**
     * Returns a vector of the default value for each parameter for the method - if there is no default
     * parameter, an empty string is returned in that position (which is different than a default parameter of
     * an empty string which would be returned as the string '""'
     */
    vector<string> get_default_argument_values(CompilerInstance & compiler_instance,
                                               const CXXMethodDecl * method,
                                               const string & annotation = "") {
        vector<string> results;
        auto parameter_count = method->getNumParams();
        for (unsigned int i = 0; i < parameter_count; i++) {
            auto param_decl = method->getParamDecl(i);

            if (param_decl->hasDefaultArg()) {
                auto default_argument = param_decl->getDefaultArg();
                auto source_range = default_argument->getSourceRange();
                auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);
                results.push_back(source);
            } else {
                results.push_back("");
            }

        }
        return results;
    }

    vector<QualType> get_method_param_qual_types(CompilerInstance & compiler_instance,
                                                 const CXXMethodDecl * method,
                                                 string const & annotation = "") {
        vector<QualType> results;
        auto parameter_count = method->getNumParams();
        for (unsigned int i = 0; i < parameter_count; i++) {
            auto param_decl = method->getParamDecl(i);


            Annotations annotations(param_decl);
            if (annotation != "" && !annotations.has(annotation)) {
                if (print_logging) cerr << "Skipping method parameter because it didn't have requested annotation: " << annotation << endl;
                continue;
            }
            auto param_qual_type = param_decl->getType();
            results.push_back(param_qual_type);
        }
        return results;
    }

    vector<string> generate_variable_names(vector<QualType> qual_types, bool with_std_move = false) {
        vector<string> results;
        for (std::size_t i = 0; i < qual_types.size(); i++) {
            if (with_std_move && qual_types[i]->isRValueReferenceType()) {
                results.push_back(fmt::format("std::move(var{})", i+1));
            } else {
                results.push_back(fmt::format("var{}", i+1));
            }
        }
        return results;
    }

    std::string get_method_parameters(CompilerInstance & compiler_instance,
                                      WrappedClass & wrapped_class,
                                      const CXXMethodDecl * method,
                                      bool add_leading_comma = false,
                                      bool insert_variable_names = false,
                                      const string & annotation = "") {
        std::stringstream result;
        bool first_param = true;
        auto type_list = get_method_param_qual_types(compiler_instance, method, annotation);

        if (!type_list.empty() && add_leading_comma) {
            result << ", ";
        }
        int count = 0;
        auto var_names = generate_variable_names(type_list, false);
        for (auto & param_qual_type : type_list) {

            if (!first_param) {
                result << ", ";
            }
            first_param = false;


            auto type_string = get_type_string(param_qual_type);
            result << type_string;

            if (insert_variable_names) {
                result << " " << var_names[count++];
            }
            update_wrapped_class_for_type(compiler_instance, wrapped_class, param_qual_type);

        }
        return result.str();
    }

    std::string get_return_type(CompilerInstance & compiler_instance,
                                WrappedClass & wrapped_class,
                                const CXXMethodDecl * method) {
        auto qual_type = method->getReturnType();
        auto result = get_type_string(qual_type);
//        auto return_type_decl = qual_type->getAsCXXRecordDecl();
//        auto full_source_loc = FullSourceLoc(return_type_decl->getLocStart(), compiler_instance);
//        auto header_file = strip_path_from_filename(compiler_instance.getFilename(full_source_loc).str());
//        if (print_logging) cerr << fmt::format("{} needs {}", wrapped_class.class_name, header_file) << endl;
//        wrapped_class.include_files.insert(header_file);
//

        update_wrapped_class_for_type(compiler_instance, wrapped_class, qual_type);

        return result;

    }


    std::string get_method_return_type_class_and_parameters(CompilerInstance & compiler_instance,
                                                            WrappedClass & wrapped_class,
                                                            const CXXRecordDecl * klass, const CXXMethodDecl * method) {
        std::stringstream results;
        results << get_return_type(compiler_instance, wrapped_class, method);
        results << ", " << get_canonical_name_for_decl(klass);
        results << get_method_parameters(compiler_instance, wrapped_class, method, true);
        return results.str();
    }

    std::string get_method_return_type_and_parameters(CompilerInstance & compiler_instance,
                                                      WrappedClass & wrapped_class,
                                                      const CXXRecordDecl * klass, const CXXMethodDecl * method) {
        std::stringstream results;
        results << get_return_type(compiler_instance, wrapped_class, method);
        results << get_method_parameters(compiler_instance, wrapped_class, method, true);
        return results.str();
    }


    void print_specialization_info(const CXXRecordDecl * decl) {
        auto name = get_canonical_name_for_decl(decl);
        cerr << "*****" << endl;
        if (decl->isDependentType()) {
            cerr << fmt::format("{} is a dependent type", name) << endl;
        }
        if (auto spec = dyn_cast<ClassTemplateSpecializationDecl>(decl)) {
            fprintf(stderr, "decl is a ClassTemplateSpecializationDecl: %p\n", (void *)decl);
            cerr << name << endl;

            if (auto spec_tmpl = spec->getSpecializedTemplate()) {
                fprintf(stderr, "Specialized template: %p, %s\n", (void *)spec_tmpl, spec_tmpl->getQualifiedNameAsString().c_str());
                print_vector(Annotations(spec_tmpl).get(), "specialized template annotations", "", false);
            } else {
                cerr << "no spec tmpl" << endl;
            }


            if (dyn_cast<ClassTemplatePartialSpecializationDecl>(decl)) {
                cerr << "It is also a partial specialization decl" << endl;
            } else {
                cerr << "It is NOT a PARTIAL specialization decl" << endl;
            }


        } else {
            cerr << name << " is not a class template specialization decl" << endl;
        }
        cerr << "*****END" << endl;

    }



    std::string get_method_string(CompilerInstance & compiler_instance,
                                  WrappedClass & wrapped_class,
                                  const CXXMethodDecl * method) {
        std::stringstream result;
        result << method->getReturnType().getAsString();

        result << method->getName().str();

        result << "(";

        result << get_method_parameters(compiler_instance, wrapped_class, method);

        result << ")";

        return result.str();
    }






    // calls callback for each constructor in the class.  If annotation specified, only
    //   constructors with that annotation will be sent to the callback
    template<class Callback>
    void foreach_constructor(const CXXRecordDecl * klass, Callback && callback,
                             const std::string & annotation = "") {
        bool print_logging = false;

        if (print_logging) cerr << "Enumerating constructors for " << klass->getNameAsString() << " with optional annotation: " << annotation << endl;

        for(CXXMethodDecl * method : klass->methods()) {
            CXXConstructorDecl * constructor = dyn_cast<CXXConstructorDecl>(method);
            bool skip = false;
            Annotations annotations(method);
            // check if method is a constructor
            if (constructor == nullptr) {
                continue;
            }

            if (print_logging) cerr << "Checking constructor: " << endl;
            if (constructor->getAccess() != AS_public) {
                if (print_logging) cerr << "  Skipping non-public constructor" << endl;
                skip = true;
            }
            if (get_export_type(constructor) == EXPORT_NONE) {
                if (print_logging) cerr << "  Skipping constructor marked for begin skipped" << endl;
                skip = true;
            }

            if (annotation != "" && !annotations.has(annotation)) {
                if (print_logging) cerr << "  Annotation " << annotation << " requested, but constructor doesn't have it" << endl;
                skip = true;
            } else {
                if (skip) {
                    if (print_logging) cerr << "  Annotation " << annotation << " found, but constructor skipped for reason(s) listed above" << endl;
                }
            }


            if (skip) {
                continue;
            } else {
                if (print_logging) cerr << " Running callback on constructor" << endl;
                callback(constructor);
            }
        }
    }
    /*

    CXXConstructorDecl * get_bidirectional_constructor(const CXXRecordDecl * klass) {
        CXXConstructorDecl * result = nullptr;
        bool got_constructor = false;
        foreach_constructor(klass, [&](auto constructor){
            if (got_constructor) {
                cerr << "ERROR, MORE THAN ONE BIDIRECTIONAL CONSTRUCTOR" << endl;
            }
            got_constructor = true;
            result = constructor;

        }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);
        if (!got_constructor) {
            cerr << "ERROR, NO BIDIRECTIONAL CONSTRUCTOR FOUND IN " << klass->getNameAsString() << endl;
        }
        return result;
    }
     */

    // This may be useful for autogenerating factory type information
//    string get_bidirectional_constructor_parameter_typelists(const CXXRecordDecl * klass, bool leading_comma) {
//        auto constructor = get_bidirectional_constructor(klass);
//
//	// all internal params must come before all external params
//	bool found_external_param = false;
//        auto parameter_count = constructor->getNumParams();
//	vector<string> internal_params;
//	vector<string> external_params;
//        for (unsigned int i = 0; i < parameter_count; i++) {
//            auto param_decl = constructor->getParamDecl(i);
//	    if (has_annotation(param_decl, V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER_STRING)) {
//		if (found_external_param) {
//		    cerr << "ERROR: Found internal parameter after external parameter found in " << klass->getNameAsString() << endl;
//		    throw std::exception();
//		}
//		internal_params.push_back(param_decl->getType().getAsString());
//	    } else {
//		found_external_param = true;
//		external_params.push_back(param_decl->getType().getAsString());
//	    }
//	}
//
//        stringstream result;
//        if (leading_comma) {
//            result << ", ";
//        }
//        result << "v8toolkit::TypeList<" << join(internal_params, ", ") << ">";
//	result << ", ";
//	result << "v8toolkit::TypeList<" << join(external_params, ", ") << ">";
//
//        return result.str();
//    }




    class BidirectionalBindings {
    private:
        CompilerInstance & compiler_instance;

        // the "normal" non-bidirectional class being wrapped
        WrappedClass & js_wrapper_class; // the bidirectional type
        WrappedClass & wrapped_class; // the non-birectional type being wrapped

    public:
        BidirectionalBindings(CompilerInstance & compiler_instance,
                              WrappedClass & js_wrapper_class,
                              WrappedClass & wrapped_class) :
            compiler_instance(compiler_instance),
            js_wrapper_class(js_wrapper_class),
            wrapped_class(wrapped_class) {}

        std::string short_name(){return wrapped_class.name_alias;}
        std::string canonical_name(){return wrapped_class.class_name;}

        std::vector<const CXXMethodDecl *> get_all_virtual_methods_for_class(const CXXRecordDecl * klass) {
            std::vector<const CXXMethodDecl *> results;
            std::deque<const CXXRecordDecl *> stack{klass};

            while (!stack.empty()) {
                auto current_class = stack.front();
                stack.pop_front();

                for(CXXMethodDecl * method : current_class->methods()) {
                    if (dyn_cast<CXXDestructorDecl>(method)) {
                        //if (print_logging) cerr << "Skipping virtual destructor while gathering virtual methods" << endl;
                        continue;
                    }
                    if (dyn_cast<CXXConversionDecl>(method)) {
                        //if (print_logging) cerr << "Skipping user-defined conversion operator" << endl;
                        continue;
                    }
                    if (method->isVirtual() && !method->isPure()) {
                        // go through existing ones and check for match
                        if (std::find_if(results.begin(), results.end(), [&](auto found){
                            if(get_method_string(compiler_instance, wrapped_class, method) ==
                               get_method_string(compiler_instance, wrapped_class, found)) {
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

            auto num_params = method->getNumParams();
//            printf("Dealing with %s\n", method->getQualifiedNameAsString().c_str());
            std::stringstream result;


            result << "  JS_ACCESS_" << num_params << (method->isConst() ? "_CONST(" : "(");

            auto return_type_string = method->getReturnType().getAsString();
            result << return_type_string << ", ";

            auto method_name = method->getName();

            result << method_name.str();

            if (num_params > 0) {
                auto types = get_method_param_qual_types(this->compiler_instance, method);
                vector<string>type_names;
                for (auto & type : types) {
                    type_names.push_back(std::regex_replace(type.getAsString(), std::regex("\\s*,\\s*"), " V8TOOLKIT_COMMA "));
                }

                result << join(type_names, ", ", true);
            }

            result  << ");\n";

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

        void generate_bindings(const std::vector<unique_ptr<WrappedClass>> & wrapped_classes) {
            std::stringstream result;
            auto matches = wrapped_class.annotations.get_regex("v8toolkit_generate_(.*)");
            if (wrapped_class.annotations.has(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {
                result << fmt::format("class JS{} : public {}, public v8toolkit::JSWrapper<{}> {{\npublic:\n", // {{ is escaped {
                                      short_name(), short_name(), short_name());
                result << fmt::format("    JS{}(v8::Local<v8::Context> context, v8::Local<v8::Object> object,\n", short_name());
                result << fmt::format("        v8::Local<v8::FunctionTemplate> created_by");
                bool got_constructor = false;
                int constructor_parameter_count;
                vector<QualType> constructor_parameters;
                foreach_constructor(wrapped_class.decl, [&](auto constructor_decl){
                    if (got_constructor) {
                        data_error(fmt::format("ERROR: Got more than one constructor for {}", wrapped_class.class_name));
                        return;
                    }
                    got_constructor = true;
                    result << get_method_parameters(compiler_instance, wrapped_class, constructor_decl, true, true);
                    constructor_parameter_count = constructor_decl->getNumParams();
                    constructor_parameters = get_method_param_qual_types(this->compiler_instance, constructor_decl);

                }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);
                if (!got_constructor) {
                    data_error(fmt::format("ERROR: Got no bidirectional constructor for {}", wrapped_class.class_name));

                }
                result << fmt::format(") :\n");

                //                auto variable_names = generate_variable_names(constructor_parameter_count);
                auto variable_names = generate_variable_names(constructor_parameters, true);

                result << fmt::format("      {}({}),\n", short_name(), join(variable_names));
                result << fmt::format("      v8toolkit::JSWrapper<{}>(context, object, created_by) {{}}\n", short_name()); // {{}} is escaped {}
                result << handle_class(wrapped_class.decl);
                result << "};\n";
            } else {
//                printf("Class %s not marked bidirectional\n", short_name().c_str());
                return;
            }

            // dumps a file per class
//            if (print_logging) cerr << "Dumping JSWrapper type for " << short_name() << endl;
            ofstream bidirectional_class_file;
            auto bidirectional_class_filename = fmt::format("v8toolkit_generated_bidirectional_{}.h", short_name());
            bidirectional_class_file.open(bidirectional_class_filename, ios::out);
            if (!bidirectional_class_file) {
                llvm::report_fatal_error(fmt::format("Could not open file: {}", bidirectional_class_filename), false);
            }

            bidirectional_class_file << "#pragma once\n\n";


            // This needs include files because the IMPLEMENTATION goes in the file (via macros).
            // If the implementation was moved out to a .cpp file, then the header file could
            //   rely soley on the primary type's includes
            for (auto & include : this->wrapped_class.include_files) {
                if (include == ""){continue;}
                bidirectional_class_file << "#include " << include << "\n";
            }


            // need to include all the includes from the parent types because the implementation of this bidirectional
            //   type may need the types for things the parent type .h files don't need (like unique_ptr contained types)
            auto all_base_type_includes = this->wrapped_class.get_base_type_includes();

            for (auto & include : all_base_type_includes) {
                std::cerr << fmt::format("for bidirectional {}, adding base type include {}", this->short_name(), include) << std::endl;
                bidirectional_class_file << "#include " << include << "\n";
            }

            bidirectional_class_file << result.str();
            bidirectional_class_file.close();


        }
    };






    map<string, int> template_instantiations;

    class ClassHandler : public MatchFinder::MatchCallback {
    private:
        CompilerInstance & ci;
        SourceManager & source_manager;

        WrappedClass * top_level_class; // the class currently being wrapped
        std::set<std::string> names_used;

    public:




        ClassHandler(CompilerInstance &CI) :
            ci(CI),
            source_manager(CI.getSourceManager())
        {}

        virtual  void onEndOfTranslationUnit () override {
            cerr << fmt::format("at end of TU with {} definitions to process", definitions_to_process.size()) << endl;

            int definition_count = 0;
            for (auto wrapped_class_ptr : definitions_to_process) {
                auto & wrapped_class = *wrapped_class_ptr;


                cerr << ++definition_count << endl;

                // all the annotations and name aliases from forward decl's and 'using' typedefs may not have been available
                // when the wrapped class was initially created, so get the full list now
                wrapped_class.update_data();


                print_specialization_info(wrapped_class.decl);

                /*
                // skipping this to try to speed things up
                if (!is_good_record_decl(klass)) {
                //cerr << "skipping 'bad' record decl" << endl;
                continue;
                }
                */
                if (wrapped_class.decl->isDependentType()) {
                    cerr << wrapped_class.class_name << endl;
                    llvm::report_fatal_error("Trying to post-process dependent type -- this should be filtered in matcher callback");
                }


                cerr << "class annotations: " << join(wrapped_class.annotations.get()) << endl;

                handle_class(wrapped_class);
            }

        }


        std::string handle_data_member(WrappedClass & containing_class, FieldDecl * field, const std::string & indentation) {
            std::stringstream result;
            auto export_type = get_export_type(field, EXPORT_ALL);
            auto short_field_name = field->getNameAsString();
            auto full_field_name = field->getQualifiedNameAsString();


            cerr << "Processing data member for: " << containing_class.name_alias << ": " << full_field_name << endl;
//            if (containing_class != top_level_class_decl) {
//                if (print_logging) cerr << "************";
//            }
//            if (print_logging) cerr << "changing data member from " << full_field_name << " to ";
//
//            std::string regex_string = fmt::format("{}::{}$", containing_class->getName().str(), short_field_name);
//            auto regex = std::regex(regex_string);
//            auto replacement = fmt::format("{}::{}", top_level_class_decl->getName().str(), short_field_name);
//            full_field_name = std::regex_replace(full_field_name, regex, replacement);
//            if (print_logging) cerr << full_field_name << endl;

            Annotations annotations(field);
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

            if (top_level_class->names.count(short_field_name)) {
                data_error(fmt::format("ERROR: duplicate name {}/{} :: {}\n",
                                       top_level_class->class_name,
                                       containing_class.class_name,
                                       short_field_name));
                return "";
            }
            top_level_class->names.insert(short_field_name);

            containing_class.fields.insert(field);

            // made up number to represent the overhead of making a new wrapped class
            //   even before adding methods/members
            // This means that two wrapped classes will count as much towards rolling to the next file as
            // one wrapped class with <THIS NUMBER> of wrapped members/functions

            update_wrapped_class_for_type(ci, *top_level_class, field->getType());

            string full_type_name = get_type_string(field->getType());

            std::cerr << fmt::format("incrementing declaration count for {} - data member", top_level_class->name_alias) << std::endl;

            if (annotations.has(V8TOOLKIT_READONLY_STRING) || field->getType().isConstQualified()) {
                result << fmt::format("{}class_wrapper.add_member_readonly<{}, {}, &{}>(\"{}\");\n", indentation,
                                      full_type_name,
                                      containing_class.class_name, full_field_name, short_field_name);
            } else {
                result << fmt::format("{}class_wrapper.add_member<{}, {}, &{}>(\"{}\");\n", indentation,
                                      full_type_name,
                                      containing_class.class_name, full_field_name, short_field_name);
            }
//            printf("%sData member %s, type: %s\n",
//                   indentation.c_str(),
//                   field->getNameAsString().c_str(),
//                   field->getType().getAsString().c_str());
            return result.str();
        }


        /*************************/
        /***** HANDLE_METHOD *****/
        /*************************/
        void handle_method(WrappedClass & klass, CXXMethodDecl * method) {


            std::string full_method_name(method->getQualifiedNameAsString());
            std::string short_method_name(method->getNameAsString());

            Annotations annotations(method);

            // check to see if there's a name annotation on the method giving it a different JavaScript name
            auto annotated_custom_name = annotations.get_regex(
                "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
            if (!annotated_custom_name.empty()) {
                short_method_name = annotated_custom_name[0];
            }


            if (print_logging || PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("Handling method: {}", full_method_name) << endl;
            //            if (print_logging) cerr << "changing method name from " << full_method_name << " to ";
//
//            auto regex = std::regex(fmt::format("{}::{}$", containing_class->getName().str(), short_method_name));
//            auto replacement = fmt::format("{}::{}", top_level_class_decl->getName().str(), short_method_name);
//            full_method_name = std::regex_replace(full_method_name, regex, replacement);
//            if (print_logging) cerr << full_method_name << endl;


            auto export_type = get_export_type(method, EXPORT_ALL);

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("Skipping method %s because not supposed to be exported %d\n",
                                                         full_method_name.c_str(), export_type);
                return;
            }

            // only deal with public methods
            if (method->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**%s is not public, skipping\n", full_method_name.c_str());
                return;
            }

            // list of overloaded operator enumerated values
            // http://llvm.org/reports/coverage/tools/clang/include/clang/Basic/OperatorKinds.def.gcov.html
            if (method->isOverloadedOperator()) {

                // if it's a call operator (operator()), grab it
                if (OO_Call == method->getOverloadedOperator()) {
                    // nothing specific to do, just don't skip it only because it's an overloaded operator
                } else {

                    // otherwise skip overloaded operators
                    if (PRINT_SKIPPED_EXPORT_REASONS)
                        printf("**skipping overloaded operator %s\n", full_method_name.c_str());
                    return;
                }
            }
            if (dyn_cast<CXXConstructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping constructor %s\n", full_method_name.c_str());
                return;
            }
            if (dyn_cast<CXXDestructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping destructor %s\n", full_method_name.c_str());
                return;
            }
            // still want to keep the interface even if it's not implemented here
            // if (method->isPure()) {
            //     if(!method->isVirtual()) {
            // 	    llvm::report_fatal_error("Got pure non-virtual method - not sure what that even means", false);
            // 	}
            //     if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), full_method_name.c_str());
            //     return;
            // }


            // If the function is wrapped in derived classes as well, you run into problems where you can't find the right type to
            //   cast the internal field to to find a match for the function type.   You may only get a Base* when you need to call void(Derived::*)()
            //   so if you only have the virtual function wrapped in Base, you'll always find the right type of object
            assert(method->isVirtual());
            assert(!method->isVirtual());
//            if (method->isVirtual()) {
//                fprintf(stderr, "%s :: %s is virtual with %d overrides\n", klass.class_name.c_str(), full_method_name.c_str(), (int)method->size_overridden_methods());
//            } else {
//                fprintf(stderr, "%s :: %s isn't virtual\n", klass.class_name.c_str(), full_method_name.c_str());
//            }
            if (method->isVirtual() && method->size_overridden_methods()) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping derived-class override of base class virtual function %s\n", full_method_name.c_str());
                return;
            }

            if (dyn_cast<CXXConversionDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("**skipping user-defined conversion operator") << endl;
                return;
            }
            if (print_logging || PRINT_SKIPPED_EXPORT_REASONS) cerr << "Method passed all checks" << endl;



            if (annotations.has(V8TOOLKIT_EXTEND_WRAPPER_STRING)) {
                // cerr << "has extend wrapper string" << endl;
                if (!method->isStatic()) {
                    data_error(fmt::format("method {} annotated with V8TOOLKIT_EXTEND_WRAPPER must be static", full_method_name.c_str()));

                }
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    cerr << fmt::format("**skipping static method marked as v8 class wrapper extension method, but will call it during class wrapping") << endl;
                top_level_class->wrapper_extension_methods.insert(full_method_name + "(class_wrapper);");

                return;
            }

            // this is VERY similar to the one above and both probably aren't needed, but they do allow SLIGHTLY different capabilities
            if (annotations.has(V8TOOLKIT_CUSTOM_EXTENSION_STRING)) {
                if (!method->isStatic()) {
                    data_error(fmt::format("method {} annotated with V8TOOLKIT_CUSTOM_EXTENSION must be static", full_method_name.c_str()));
                }
                if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("**skipping static method marked as V8TOOLKIT_CUSTOM_EXTENSION, but will call it during class wrapping") << endl;
                top_level_class->wrapper_custom_extensions.insert(fmt::format("class_wrapper.add_new_constructor_function_template_callback(&{});", full_method_name));
            }



            //	    cerr << "Checking if method name already used" << endl;
            if (top_level_class->names.count(short_method_name)) {
                data_error(fmt::format("Skipping duplicate name {}/{} :: {}\n",
                                       top_level_class->class_name,
                                       klass.class_name,
                                       short_method_name));
                return;
            }
            //cerr << "Inserting short name" << endl;
            top_level_class->names.insert(short_method_name);

            auto parsed_method = new ParsedMethod(this->ci, klass, method);


            if (method->isStatic()) {
                cerr << "method is static" << endl;
                std::cerr << fmt::format("incrementing declaration count for {} - static function", top_level_class->name_alias) << std::endl;
                top_level_class->declaration_count++;

                if (static_method_renames.find(short_method_name) != static_method_renames.end()) {
                    short_method_name = static_method_renames[short_method_name];
                }

//                result << fmt::format("class_wrapper.add_static_method<{}>(\"{}\", &{});\n",
//                                      get_method_return_type_and_parameters(ci, *top_level_class, klass.decl, method),
//                                      short_method_name, full_method_name);
//                klass.methods.insert(parsed_method);
                klass.has_static_method = true;
            } else {
                cerr << "Method is not static" << endl;
                std::cerr << fmt::format("incrementing declaration count for {} - non-static function", top_level_class->name_alias) << std::endl;
                top_level_class->declaration_count++;
                klass.methods.insert(parsed_method);

                // overloaded operator type names (like OO_Call) defined here:
                //   http://llvm.org/reports/coverage/tools/clang/include/clang/Basic/OperatorKinds.def.gcov.html
                // name is "OO_" followed by the first field in each line
                if (OO_Call == method->getOverloadedOperator()) {
//                    result << fmt::format("class_wrapper.make_callable<{}>(&{});\n",
//                                          get_method_return_type_class_and_parameters(ci, *top_level_class, klass.decl,
//                                                                                      method),
//                                          full_method_name);
                } else {
//                    result << fmt::format("class_wrapper.add_method<{}>(\"{}\", &{});\n",
//                                          get_method_return_type_class_and_parameters(ci, *top_level_class, klass.decl,
//                                                                                      method),
//                                          short_method_name, full_method_name);
                }
                methods_wrapped++;

            }
        } // end handle_method



        void handle_class(WrappedClass & wrapped_class, // class currently being handled (not necessarily top level)
                          bool top_level = true,
                          const std::string & indentation = "") {


            // update the includes for the type for itself -- if it's a templated type, make sure it has includes for the types it is
            //   templated with
            update_wrapped_class_for_type(ci, wrapped_class,
                                          wrapped_class.decl->getTypeForDecl()->getCanonicalTypeInternal());
            PrintLoggingGuard lg;
            if (top_level) {

                if (std::regex_search(wrapped_class.class_name, std::regex("^(class|struct)\\s+v8toolkit"))) {
                    lg.log();
                }


                if (!wrapped_class.should_be_wrapped()) {
                    cerr << fmt::format("Skipping {}, should_be_wrapped reported false", wrapped_class.class_name)
                         << endl;
                    return;
                }

                // if we've gotten this far, then the class passed the tests and should be included
                wrapped_class.valid = true;


                top_level_class = &wrapped_class;


                cerr << "**** In top-level handle_class with type: " << wrapped_class.class_name << endl;


                for (auto &ignore_regex : types_to_ignore_regex) {
                    if (std::regex_search(wrapped_class.class_name, std::regex(ignore_regex))) {
                        cerr << "skipping top level class because it is in types_to_ignore_regex list: "
                             << wrapped_class.class_name << endl;
                        wrapped_class.valid = false;
                        return;
                    }
                }


                if (wrapped_class.done) {
                    cerr << "Already processed top level class" << endl;
                    return;
                }
                wrapped_class.done = true;

                if (dyn_cast<ClassTemplatePartialSpecializationDecl>(wrapped_class.decl)) {
                    cerr << "is class template partial specilziation decl" << endl;
                }


                if (wrapped_class.decl->getTypeForDecl()->isDependentType()) {
                    if (print_logging) cerr << "Skipping dependent type top-level class" << endl;
                    wrapped_class.valid = false;
                    return;
                }
                const ClassTemplateSpecializationDecl *specialization = nullptr;
                if ((specialization = dyn_cast<ClassTemplateSpecializationDecl>(wrapped_class.decl)) != nullptr) {
                    auto specialized_template = specialization->getSpecializedTemplate();
                    auto template_name = specialized_template->getNameAsString();
                    if (template_name == "remove_reference") {
                        cerr << wrapped_class.class_name << endl;
                    }
                    template_instantiations[template_name]++;
                }


#ifdef TEMPLATE_INFO_ONLY
                return;
#endif
                if (!is_good_record_decl(wrapped_class.decl)) {
                    if (true || print_logging) cerr << "Skipping 'bad' CXXRecordDecl" << endl;
                    wrapped_class.valid = false;
                    return;
                }


                classes_wrapped++;
                names_used.clear();

                //                printf("Handling top level class %s\n", top_level_class->class_name.c_str());


                if (print_logging)
                    cerr << "Adding include for class being handled: " << wrapped_class.class_name << " : "
                         << get_include_for_type_decl(ci, wrapped_class.decl) << endl;

                // get the .h file the type is defined in
                wrapped_class.include_files.insert(get_include_for_type_decl(this->ci, wrapped_class.decl));


            } // end if top level class
            else {
                cerr << fmt::format("{} Handling class (NOT top level) {}", indentation, wrapped_class.class_name)
                     << endl;
            }

            auto class_name = wrapped_class.decl->getQualifiedNameAsString();

            // prints out source for decl
            //fprintf(stderr,"class at %s", decl2str(klass,  this->ci).c_str());

            auto full_source_loc = FullSourceLoc(wrapped_class.decl->getLocation(), this->source_manager);

            auto file_id = full_source_loc.getFileID();

//            fprintf(stderr,"%sClass/struct: %s\n", indentation.c_str(), class_name.c_str());
            // don't do the following code for inherited classes
            if (top_level) {
                top_level_class->include_files.insert(get_include_string_for_fileid(this->ci, file_id));
            }

//            fprintf(stderr,"%s Decl at line %d, file id: %d %s\n", indentation.c_str(), full_source_loc.getExpansionLineNumber(),
//                   full_source_loc.getFileID().getHashValue(), this->ci.getBufferName(full_source_loc));

//                auto type_decl = dyn_cast<TypeDecl>(klass);
//                assert(type_decl);
//                auto type = type_decl->getTypeForDecl();

//
            // non-top-level methods handled by javascript prototype
            if (top_level) {
                if (print_logging) cerr << "About to process methods" << endl;
                for (CXXMethodDecl *method : wrapped_class.decl->methods()) {
                    if (method->hasInheritedPrototype()) {
                        cerr << fmt::format("Skipping method {} because it has inherited prototype",
                                            method->getNameAsString()) << endl;
                        continue;
                    }

                    handle_method(*top_level_class, method);
                }
            }

            if (print_logging) cerr << "About to process fields for " << wrapped_class.name_alias << endl;
            for (FieldDecl *field : wrapped_class.decl->fields()) {
                top_level_class->members.insert(handle_data_member(wrapped_class, field, indentation + "  "));
            }

            if (print_logging) cerr << "About to process base class info" << endl;
            // if this is true and the type ends up with no base type, it's an error
            bool must_have_base_type = false;
            auto annotation_base_types_to_ignore = wrapped_class.annotations.get_regex(
                "^" V8TOOLKIT_IGNORE_BASE_TYPE_PREFIX "(.*)$");
            auto annotation_base_type_to_use = wrapped_class.annotations.get_regex(
                "^" V8TOOLKIT_USE_BASE_TYPE_PREFIX "(.*)$");
            if (annotation_base_type_to_use.size() > 1) {
                data_error(fmt::format("More than one base type specified to use for type", wrapped_class.class_name));
            }

            // if a base type to use is specified, then it must match an actual base type or error
            if (!annotation_base_type_to_use.empty()) {
                must_have_base_type = true;
            }


            print_vector(annotation_base_types_to_ignore, "base types to ignore");
            print_vector(annotation_base_type_to_use, "base type to use");


            bool found_base_type = false;
            if (print_logging) cerr << "About to process base classes" << endl;
            for (auto base_class : wrapped_class.decl->bases()) {

                auto base_qual_type = base_class.getType();
                auto base_type_decl = base_qual_type->getAsCXXRecordDecl();
                auto base_type_name = base_type_decl->getNameAsString();
                auto base_type_canonical_name = get_canonical_name_for_decl(base_type_decl);

                if (base_type_canonical_name == "class v8toolkit::WrappedClassBase" &&
                    base_class.getAccessSpecifier() != AS_public) {
                    data_error(fmt::format("class inherits from v8toolkit::WrappedClassBase but not publicly: {}",
                                           wrapped_class.class_name).c_str());
                }

                cerr << "Base type: " << base_type_canonical_name << endl;
                if (std::find(annotation_base_types_to_ignore.begin(), annotation_base_types_to_ignore.end(),
                              base_type_canonical_name) !=
                    annotation_base_types_to_ignore.end()) {
                    cerr << "Skipping base type because it was explicitly excluded in annotation on class: "
                         << base_type_name << endl;
                    continue;
                } else {
                    cerr << "Base type was not explicitly excluded via annotation" << endl;
                }
                if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), base_type_canonical_name) !=
                    base_types_to_ignore.end()) {
                    cerr << "Skipping base type because it was explicitly excluded in plugin base_types_to_ignore: "
                         << base_type_name << endl;
                    continue;
                } else {
                    cerr << "Base type was not explicitly excluded via global ignore list" << endl;
                }
                if (!annotation_base_type_to_use.empty() && annotation_base_type_to_use[0] != base_type_name) {
                    cerr << "Skipping base type because it was not the one specified to use via annotation: "
                         << base_type_name << endl;
                    continue;
                }

                if (base_qual_type->isDependentType()) {
                    cerr << indentation << "-- base type is dependent" << endl;
                }


                found_base_type = true;
                auto base_record_decl = base_qual_type->getAsCXXRecordDecl();

//                fprintf(stderr, "%s -- type class: %d\n", indentation.c_str(), base_qual_type->getTypeClass());
//                cerr << indentation << "-- base type has a cxxrecorddecl" << (record_decl != nullptr) << endl;
//                cerr << indentation << "-- base type has a tagdecl: " << (base_tag_decl != nullptr) << endl;
//                cerr << indentation << "-- can be cast to tagtype: " << (dyn_cast<TagType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to attributed type: " << (dyn_cast<AttributedType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to injected class name type: " << (dyn_cast<InjectedClassNameType>(base_qual_type) != nullptr) << endl;


                if (base_record_decl == nullptr) {
                    llvm::report_fatal_error("Got null base type record decl - this should be caught ealier");
                }
                //  printf("Found parent/base class %s\n", record_decl->getNameAsString().c_str());

                cerr << "getting base type wrapped class object" << endl;
                WrappedClass &current_base = get_or_insert_wrapped_class(base_record_decl, this->ci,
                                                                         FOUND_BASE_CLASS);

                // if the base type hasn't been independently processed, do that right now
                if (!current_base.done) {
                    cerr << fmt::format("interupting handle_class of {} to fully handle class of base type: {}",
                                        wrapped_class.class_name, current_base.class_name) << endl;
                    auto top_level_class_backup = top_level_class;
                    top_level_class = &current_base;
                    handle_class(current_base, true);
                    top_level_class = top_level_class_backup;
                }

                auto current_base_include = get_include_for_type_decl(this->ci, current_base.decl);
                auto current_include = get_include_for_type_decl(this->ci, wrapped_class.decl);
                //                printf("For %s, include %s -- for %s, include %s\n", current_base->class_name.c_str(), current_base_include.c_str(), current->class_name.c_str(), current_include.c_str());

                wrapped_class.include_files.insert(current_base_include);
                current_base.include_files.insert(current_include);
                wrapped_class.base_types.insert(&current_base);
                current_base.derived_types.insert(&wrapped_class);

                //printf("%s now has %d base classes\n", current->class_name.c_str(), (int)current->base_types.size());
                //printf("%s now has %d derived classes\n", current_base->class_name.c_str(), (int)current_base->derived_types.size());

                handle_class(current_base, false, indentation + "  ");
            }

            if (print_logging) cerr << "done with base classes" << endl;
            if (must_have_base_type && !found_base_type) {
                data_error(
                    fmt::format("base_type_to_use specified but no base type found: {}", wrapped_class.class_name));
            }



            // Only process constructors on the top-level object
            if (top_level) {
                if (wrapped_class.found_method == FOUND_BASE_CLASS) {
                    cerr << fmt::format(
                        "Not wrapping constructor for class only wrapped because it's base class of another wrapped type")
                         << endl;
                } else if (wrapped_class.annotations.has(V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS_STRING)) {
                    cerr << fmt::format("Not wrapping because class has DO NOT WRAP CONSTRUCTORS annotation") << endl;
                } else if (wrapped_class.decl->isAbstract()) {
                    cerr << "Skipping all constructors because class is abstract: " << class_name << endl;
                } else if (wrapped_class.force_no_constructors) {
                    cerr << fmt::format("'force no constructors' set on {} so skipping making constructors", class_name)
                         << endl;
                } else {
                    if (print_logging)
                        cerr << fmt::format(
                            "About to process constructors for {} -- passed all checks to skip class constructors.",
                            wrapped_class.class_name) << endl;

                    foreach_constructor(wrapped_class.decl, [&](auto constructor) {

//                        auto full_source_loc = FullSourceLoc(constructor->getLocation(), this->ci);
//                        fprintf(stderr,"%s %s constructor Decl at line %d, file id: %d %s\n", indentation.c_str(),
//                                top_level_class_decl->getName().str().c_str(),
//                                full_source_loc.getExpansionLineNumber(),
//                                full_source_loc.getFileID().getHashValue(),
//                                this->ci.getBufferName(full_source_loc));


                        if (constructor->isCopyConstructor()) {
                            fprintf(stderr, "Skipping copy constructor\n");
                            return;
                        } else if (constructor->isMoveConstructor()) {
                            fprintf(stderr, "Skipping move constructor\n");
                            return;
                        } else if (constructor->isDeleted()) {
                            if (print_logging) cerr << "Skipping deleted constructor" << endl;
                            return;
                        }
                        Annotations annotations(constructor);
                        auto constructor_name_annotation = annotations.get_regex(V8TOOLKIT_CONSTRUCTOR_PREFIX "(.*)");
                        // fprintf(stderr,"Got %d annotations on constructor\n", (int)annotations.size());
                        std::string constructor_name = wrapped_class.name_alias;
                        if (!constructor_name_annotation.empty()) {
                            constructor_name = constructor_name_annotation[0];
                        }
                        if (std::find(used_constructor_names.begin(), used_constructor_names.end(), constructor_name) !=
                            used_constructor_names.end()) {
                            data_error(
                                fmt::format("Error: because duplicate JS constructor function name: {} in class {}",
                                            constructor_name.c_str(), wrapped_class.class_name));
                            for (auto &name : used_constructor_names) {
                                cerr << (fmt::format("Already used constructor name: {}", name)) << endl;
                            }
                        } else {
                            cerr << fmt::format("for {}, wrapping constructor {}", wrapped_class.class_name,
                                                constructor_name) << endl;
                            used_constructor_names.push_back(constructor_name);

                            top_level_class->constructors.insert(
                                fmt::format("{}  class_wrapper.add_constructor<{}>(\"{}\", isolate);\n",
                                            indentation, get_method_parameters(this->ci,
                                                                               *top_level_class,
                                                                               constructor), constructor_name));
                        }
                    });

                }
                // if there's no constructor but there is a static method, then add the special command to expose
                //   the static methods
                if (top_level_class->constructors.empty() && top_level_class->has_static_method) {
                    std::string static_name = wrapped_class.name_alias;
                    auto static_name_annotation = top_level_class->annotations.get_regex(
                        V8TOOLKIT_EXPOSE_STATIC_METHODS_AS_PREFIX "(.*)");
                    if (!static_name_annotation.empty()) {
                        static_name = static_name_annotation[0];
                    }
                    top_level_class->constructors.insert(
                        fmt::format("{}  class_wrapper.expose_static_methods(\"{}\", isolate);\n", indentation,
                                    static_name)
                    );
                }

                // if this is a bidirectional class, make a minimal wrapper for it
                if (wrapped_class.annotations.has(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {

                    if (print_logging)
                        cerr << "Type " << top_level_class->class_name << " **IS** bidirectional" << endl;

                    auto generated_header_name = fmt::format("\"v8toolkit_generated_bidirectional_{}.h\"",
                                                             top_level_class->get_short_name());


                    auto bidirectional_class_name = fmt::format("JS{}", top_level_class->get_short_name());
                    // auto js_wrapped_classes = get_wrapped_class_regex(bidirectional_class_name + "$"); SEE COMMENT BELOW
                    WrappedClass *js_wrapped_class = nullptr;
                    //if (js_wrapped_classes.empty()) { SEE COMMENT BELOW
                    cerr << "Creating new Wrapped class object for " << bidirectional_class_name << endl;
                    auto bidirectional_unique_ptr = std::make_unique<WrappedClass>(bidirectional_class_name,
                                                                                   this->ci);
                    js_wrapped_class = bidirectional_unique_ptr.get();
                    wrapped_classes.emplace_back(move(bidirectional_unique_ptr));

                    auto &bidirectional = *js_wrapped_class;
                    bidirectional.base_types.insert(top_level_class);

                    cerr << fmt::format("Adding derived bidirectional type {} to base type: {}",
                                        bidirectional.class_name, wrapped_class.name_alias) << endl;

                    // set the bidirectional class as being a subclass of the non-bidirectional type
                    wrapped_class.derived_types.insert(&bidirectional);

                    bidirectional.include_files.insert(generated_header_name);
                    bidirectional.my_include = generated_header_name;

                    BidirectionalBindings bd(this->ci, *js_wrapped_class, wrapped_class);
                    bd.generate_bindings(wrapped_classes);


                } else {
                    if (print_logging)
                        cerr << "Type " << top_level_class->class_name << " is not bidirectional" << endl;
                }

            } // if top level
        } // end method






        /**
         * This runs per-match from MyASTConsumer, but always on the same ClassHandler object
         */
        virtual void run(const MatchFinder::MatchResult &Result) override {

            matched_classes_returned++;

            if (matched_classes_returned % 10000 == 0) {
                cerr << endl << "### MATCHER RESULT " << matched_classes_returned << " ###" << endl;
            }

            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("not std:: class")) {
                auto class_name = get_canonical_name_for_decl(klass);

                if (klass->isDependentType()) {
                    cerr << "Skipping 'class with annotation' dependent type: " << class_name << endl;
                    return;
                }

                auto name = get_canonical_name_for_decl(klass);
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
                    return;
                }
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
                    return;
                }

                cerr << endl << "Got class definition: " << class_name << endl;
                fprintf(stderr, "decl ptr: %p\n", (void *)klass);


                if (!is_good_record_decl(klass)) {
                    cerr << "SKIPPING BAD RECORD DECL" << endl;
                }

                cerr << "Storing it for later processing (unless dupe)" << endl;



                add_definition(get_or_insert_wrapped_class(klass, this->ci, FOUND_UNSPECIFIED));
            }

            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("forward declaration with annotation")) {
                auto class_name = get_canonical_name_for_decl(klass);
                cerr << endl << "Got forward declaration with annotation: " << class_name << endl;
                fprintf(stderr, "decl ptr: %p\n", (void *)klass);

                cerr << get_include_for_type_decl(this->ci, klass) << endl;

                print_specialization_info(klass);

                print_vector(Annotations(klass).get(), "annotations directly on forward declaration");

                /*
                if (klass->getTemplateInstantiationPattern()) {
                    auto pattern = klass->getTemplateInstantiationPattern();
                    fprintf(stderr, "pattern decl ptr: %p, %s\n", (void *)pattern, get_canonical_name_for_decl(pattern).c_str());

                } else {
                    fprintf(stderr, "no instantiation pattern on this type\n");
                }


                if (auto tmpl = klass->getDescribedClassTemplate()) {
                    fprintf(stderr, "Described class template ptr: %p, %s\n", (void *)tmpl, tmpl->getQualifiedNameAsString().c_str());
                } else {
                    cerr << "no described class template" << endl;
                }

                cerr << "forward declaration annotations" << endl;
                cerr << join(Annotations(klass).get()) << endl;

                */

                /* check to see if this has any annotations we should associate with its associated template */
                auto described_tmpl = klass->getDescribedClassTemplate();
                if (klass->isDependentType() && described_tmpl) {
                    fprintf(stderr, "described template %p, %s\n", (void *)described_tmpl, described_tmpl->getQualifiedNameAsString().c_str());
                    printf("Merging %d annotations with template %p\n", (int)Annotations(klass).get().size(), (void*)described_tmpl);
                    annotations_for_class_templates[described_tmpl].merge(Annotations(klass));
                }

#if 0
                // if theres no definition, it's possible the class only has specializations, so just skip this
                if (!klass->hasDefinition()) {
		    return;
                    //llvm::report_fatal_error(fmt::format("Class {} only has a forward declaration, no definition anywhere", class_name).c_str());
                }

		
                // go from the forward declaration to the real type
                klass = klass->getDefinition();


		auto & wrapped_class = get_or_insert_wrapped_class(klass, this->ci, FOUND_ANNOTATION);

		cerr << "class annotations" << endl;
		cerr << join(wrapped_class.annotations.get()) << endl;

		
		if (!klass->isThisDeclarationADefinition()) {
		    llvm::report_fatal_error("Got forward decl, called getDefinition, but it still wasn't a definition.. don't know how to handle that");
		}
		//		cerr << "definition declaration annotations" << endl;
		//		cerr << join(Annotations(klass).get()) << endl;

		if (!is_good_record_decl(klass)) {
		    cerr << "skipping 'bad' record decl" << endl;
		    return;
		}
		if (klass->isDependentType()) {
		    cerr << "skipping dependent type" << endl;

		    return;
		}

		if (!wrapped_class.should_be_wrapped()) {
		    return;
		}


                if (klass == nullptr || !klass->isCompleteDefinition() || class_name != get_canonical_name_for_decl(klass)) {
                    llvm::report_fatal_error(fmt::format("Class {} - not sure what's going on with it couldn't get definition or name changed", class_name).c_str());
                }

                handle_class(wrapped_class);
#endif

            }
            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class derived from WrappedClassBase")) {
                cerr << endl << "Got class derived from v8toolkit::WrappedClassBase: " << get_canonical_name_for_decl(klass) << endl;
                if (!is_good_record_decl(klass)) {
                    cerr << "skipping 'bad' record decl" << endl;
                    return;
                }
                if (klass->isDependentType()) {
                    cerr << "skipping dependent type" << endl;
                    return;
                }

                auto name = get_canonical_name_for_decl(klass);
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
                    return;
                }
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
                    return;
                }


                if (Annotations(klass).has(V8TOOLKIT_NONE_STRING)) {
                    cerr << "Skipping class because it's explicitly marked SKIP" << endl;
                    return;
                }


                print_specialization_info(klass);


                if (!is_good_record_decl(klass)) {
                    cerr << "SKIPPING BAD RECORD DECL" << endl;
                }

                cerr << "Storing it for later processing (unless dupe)" << endl;
                add_definition(get_or_insert_wrapped_class(klass, this->ci, FOUND_INHERITANCE));
            }

            // only pick off the typedefNameDecl entries, but in 3.8, typedefNameDecl() matcher isn't available
            if (auto typedef_decl = Result.Nodes.getNodeAs<clang::TypedefNameDecl>("named decl")) {
                auto qual_type = typedef_decl->getUnderlyingType();
                auto record_decl = qual_type->getAsCXXRecordDecl();

                // not interesting - it's for something like a primitive type like 'long'
                if (!record_decl) {
                    return;
                }
                auto name = get_canonical_name_for_decl(record_decl);
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
                    return;
                }
                if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
                    return;
                }


                cerr << endl << "^^^^^^^^^********* GOT TYPEDEF MATCHED" << endl;
                cerr << "qualified name: " << typedef_decl->getQualifiedNameAsString() << endl;
                fprintf(stderr, "record decl ptr: %p\n", (void *)record_decl);
                cerr << "include file: " << get_include_for_type_decl(this->ci, typedef_decl) << endl;
                cerr << "Annotations: " << join(Annotations(typedef_decl).get()) << endl;

                cerr << "Using type: " << get_type_string(qual_type) << endl;

                cerr << "Canonical name for type being used: " << name << endl;

                print_specialization_info(record_decl);

                annotations_for_record_decls[record_decl].merge(Annotations(typedef_decl));


                if (Annotations(typedef_decl).has(V8TOOLKIT_NAME_ALIAS_STRING)) {
                    fprintf(stderr, "Setting alias for %p to %s\n", (void *)record_decl, typedef_decl->getNameAsString().c_str());
                    names_for_record_decls[record_decl] = typedef_decl->getNameAsString();
                }

            }

#ifdef TEMPLATE_INFO_ONLY

            if (const ClassTemplateSpecializationDecl * klass = Result.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>("class")) {
		auto class_name = get_canonical_name_for_decl(klass);

		bool print_logging = false;
		
		if (std::regex_search(class_name, std::regex("^(class|struct)\\s+v8toolkit"))) {
		//		if (std::regex_search(class_name, std::regex("remove_reference"))) {
		    print_logging = true;
		    cerr << fmt::format("Got class {}", class_name) << endl;
		}


#ifdef TEMPLATE_FILTER_STD
		if (std::regex_search(class_name, std::regex("^std::"))) {
		    if (print_logging) cerr << "Filtering out because in std::" << endl;
		    return;
		}
#endif


		
		auto tmpl = klass->getSpecializedTemplate();
		if (print_logging) {
		    cerr << "got specialized template " << tmpl->getQualifiedNameAsString() << endl;
		}



#ifdef TEMPLATE_FILTER_STD
		if (std::regex_search(tmpl->getQualifiedNameAsString(), std::regex("^std::"))) {
		    return;
		}
#endif

		
		ClassTemplate::get_or_create(tmpl).instantiated();

		
	    }
	    
	    if (const CXXMethodDecl * method = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("method")) {
		auto method_name = method->getQualifiedNameAsString();
		const FunctionDecl * pattern = nullptr;
		
		if (!method->isTemplateInstantiation()) {
		    return;
		}
#ifdef TEMPLATE_FILTER_STD
		if (std::regex_search(method_name, std::regex("^std::"))) {
		    return;
		}
#endif
		
		pattern = method->getTemplateInstantiationPattern();
		if (!pattern) {
		    pattern = method;
		}
		    
		if (!pattern) {
		    llvm::report_fatal_error("method is template insantiation but pattern still nullptr");
		}
		
		FunctionTemplate::get_or_create(pattern).instantiated();


#if 0
		bool print_logging = false;

		if (std::regex_search(method_name, std::regex("function_in_temp"))) {
		    cerr << endl << "*******Found function in templated class decl" << endl;
		    fprintf(stderr, "Method decl ptr: %p\n", (void*) method);
		    cerr << "is dependent context: " << method->isDependentContext() << endl;
		    cerr << "has dependent template info: " << (method->getDependentSpecializationInfo() != nullptr) << endl;
		    cerr << "is template instantiation: " << (method->isTemplateInstantiation()) << endl;
		    cerr << "has instantiation pattern: " << (method->getTemplateInstantiationPattern() != nullptr) << endl;
		    if (method->getTemplateInstantiationPattern()) {
			fprintf(stderr, "template instantiation pattern ptr: %p\n", (void*) method->getTemplateInstantiationPattern());
		    }
		    print_logging = true;
		}

		const FunctionTemplateDecl * function_template_decl = method->getDescribedFunctionTemplate();

		if (function_template_decl == nullptr && method->getTemplateSpecializationInfo()) {
		    function_template_decl = method->getTemplateSpecializationInfo()->getTemplate();
		}

		if (function_template_decl) {
		    cerr << fmt::format("'real' templated method {} has instantiation pattern: {}", method_name, method->getTemplateInstantiationPattern() != nullptr) << endl;
		    fprintf(stderr, "method: %p, instantiation pattern: %p\n", (void *)method, (void*)method->getTemplateInstantiationPattern());
		    if (print_logging)
			cerr << fmt::format("Got method {}", method_name) << endl;
		    FunctionTemplate::get_or_create(function_template_decl).instantiated();
		} else {
		    if (print_logging) cerr << "not interesting method" << endl;
		}
		return;

#endif
		
	    }
#endif // end TEMPLATE_INFO_ONLY
        }
    };



#include "ast_consumer.h"



#include "ast_action.h"
}

static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
    X("v8toolkit-generate-bindings", "generate v8toolkit bindings");
