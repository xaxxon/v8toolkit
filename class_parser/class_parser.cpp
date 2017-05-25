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

// Any base types you want to always ignore -- v8toolkit::WrappedClassBase must remain, others may be added/changed
vector<string> base_types_to_ignore = {"class v8toolkit::WrappedClassBase", "class Subscriber"};


// Top level types that will be immediately discarded
vector<string> types_to_ignore_regex = {"^struct has_custom_process[<].*[>]::mixin$"};

vector<string> includes_for_every_class_wrapper_file = {"\"js_casts.h\"", "<v8toolkit/v8_class_wrapper_impl.h>"};

// error if bidirectional types don't make it in due to include file ordering
// disable "fast_compile" so the V8ClassWrapper code can be generated 
string header_for_every_class_wrapper_file = "#define NEED_BIDIRECTIONAL_TYPES\n#undef V8TOOLKIT_WRAPPER_FAST_COMPILE\n";

// sometimes files sneak in that just shouldn't be
vector<string> never_include_for_any_file = {"\"v8helpers.h\""};



map<string, string> static_method_renames = {{"name", "get_name"}};

// http://usejsdoc.org/tags-type.html
map<string, string> cpp_to_js_type_conversions = {{"^(?:std::|eastl)?vector", "Array.{$1}"},
                                                  {"^(?:std::|eastl::)?(?:vector_)?(?:multi)?map", "Object.{$1, $2}"},
                                                  //{"^([^<]+)\\s*[<]\\s*(.+?)\\s*[>]\\s*([^>]*)$", "$1<$2>$3"},
                                                  {"^(?:const)?\\s*(?:unsigned)?\\s*(?:char|short|int|long|long long|float|double|long double)\\s*(?:const)?\\s*[*]?\\s*[&]?$", "Number"},
                                                  {"^(?:const)?\\s*_?[Bb]ool\\s*(?:const)?\\s*[*]?\\s*[&]?$", "Boolean"},
                                                  {"^(?:const)?\\s*(?:char\\s*[*]|(?:std::)?string)\\s*(?:const)?\\s*\\s*[&]?$", "String"},
                                                  {"^void$", "Undefined"},
                                                  {"^(?:std::)?unique_ptr", "$1"},
                                                  {"^(?:std::)?basic_string", "String"},
                                                  {"^\\s*nullptr\\s*$", "null"}
};

// regex for @callback instead of @param: ^(const)?\s*(std::)?function[<][^>]*[>]\s*(const)?\s*\s*[&]?$

std::string js_api_header = R"JS_API_HEADER(

/**
 * @type World
 */
var world;

/**
 * @type Map
 */
var map;

/**
 * @type Game
 */
var game;

/**
 * Prints a string and appends a newline
 * @param {String} s the string to be printed
 */
function println(s){}

/**
 * Prints a string without adding a newline to the end
 * @param {String} s the string to be printed
 */
function print(s){}

/**
 * Dumps the contents of the given variable - only 'own' properties
 * @param o {Object} the variable to be dumped
 */
function printobj(o){}

/**
 * Dumps the contents of the given variable - all properties including those of prototype chain
 * @param o {Object} the variable to be dumped
 */
function printobjall(o){}

/**
 * Attempts to load the given module and returns the exported data.  Requiring the same module
 *   more than once will return the cached result, not re-execute the source.
 * @param {String} module_name name of the module to require
 */
function require(module_name){}


)JS_API_HEADER";

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
#include <regex>

#include <fmt/ostream.h>

#include "clang.h"

#include "class_parser.h"



int print_logging = 1;


#include "wrapped_class.h"
#include "ast_action.h"
#include "ast_consumer.h"
#include "annotations.h"
#include "class_handler.h"
#include "parsed_method.h"


int matched_classes_returned = 0;

vector<WrappedClass *> WrappedClass::wrapped_classes;



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


void print_vector(const vector<string> & vec, const string & header, const string & indentation, bool ignore_empty) {

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

    for (auto & wrapped_class : WrappedClass::wrapped_classes) {

        if (wrapped_class->class_name == class_name) {
            return true;
        }
    }
    return false;
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
                            const std::string & indentation) {

    auto original_qualifiers = qual_type.getLocalFastQualifiers();
    // chase any typedefs to get the "real" type
    while (auto typedef_type = dyn_cast<TypedefType>(qual_type)) {
        qual_type = typedef_type->getDecl()->getUnderlyingType();
    }

    // re-apply qualifiers to potentially new qualtype
    qual_type.setLocalFastQualifiers(original_qualifiers);

    auto canonical_qual_type = qual_type.getCanonicalType();
//        cerr << "canonical qual type typeclass: " << canonical_qual_type->getTypeClass() << endl;
    static PrintingPolicy pp = PrintingPolicy(LangOptions());
    pp.adjustForCPlusPlus();
    auto canonical = canonical_qual_type.getAsString(pp);
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


QualType get_substitution_type_for_type(QualType original_type, map<string, QualType> template_types) {

    if (!original_type->isDependentType()) {
        return original_type;
    }

    std::cerr << fmt::format("in get_substitution_type_for_type for {}, have {} template_type options", original_type.getAsString(), template_types.size()) << std::endl;

    QualType current_type = original_type;


    current_type = current_type.getNonReferenceType();

    bool changed = true;
    while(changed) {
        changed = false;
        if (current_type.isConstQualified()) {
            current_type.removeLocalConst();
        }

        // if it's a pointer...
        if (!current_type->getPointeeType().isNull()) {
            changed = true;
            current_type = current_type->getPointeeType();
            continue; // check for more pointers first
        }

        // This code traverses all the typdefs and pointers to get to the actual base type
        if (dyn_cast<TypedefType>(current_type) != nullptr) {
            changed = true;
            if (print_logging) cerr << "stripped typedef, went to: " << current_type.getAsString() << endl;
            current_type = dyn_cast<TypedefType>(current_type)->getDecl()->getUnderlyingType();
        }
    }

    auto i = template_types.find(current_type.getAsString());

    if (i == template_types.end()) {
        // if this is being called, a stripped down type is fine, so ship back the stripped down original type
        return current_type;
    } else {
        return i->second;
    }
}


std::string substitute_type(QualType original_type, map<string, QualType> template_types) {

    if (!original_type->isDependentType()) {
        auto result = get_type_string(original_type);
        std::cerr << fmt::format("Not dependent type, short circuiting template substitution: {}", result) << std::endl;
        return result;
    }


    std::cerr << fmt::format("Substituting dependent type: {} with template_types size: {}", original_type.getAsString(),
    template_types.size()) << std::endl;
    std::string suffix;





//********************************************************************************



    QualType current_type = original_type;

    if (current_type->isLValueReferenceType()) {
        suffix = " &";
    } else if (current_type->isRValueReferenceType()) {
        suffix = " &&";
    }
    current_type = current_type.getNonReferenceType();

    bool changed = true;
    while (changed) {
        changed = false;
        if (current_type.isConstQualified()) {
            suffix = std::string(" const") + suffix;
            current_type.removeLocalConst();
        }

        // if it's a pointer...
        if (!current_type->getPointeeType().isNull()) {
            changed = true;
            suffix = std::string(" *") + suffix;
            current_type = current_type->getPointeeType();
            continue; // check for more pointers first
        }

        // This code traverses all the typdefs and pointers to get to the actual base type
        if (dyn_cast<TypedefType>(current_type) != nullptr) {
            changed = true;
            if (print_logging) cerr << "stripped typedef, went to: " << current_type.getAsString() << endl;
            current_type = dyn_cast<TypedefType>(current_type)->getDecl()->getUnderlyingType();
        }
    }

    std::cerr << fmt::format("Got down to: {}", current_type.getAsString()) << std::endl;

    auto qual_type = get_substitution_type_for_type(current_type, template_types);

    std::cerr << fmt::format("after substitution: {}", qual_type.getAsString()) << std::endl;



    // strip any references
    qual_type = qual_type.getLocalUnqualifiedType();


    // remove pointers
    while(!qual_type->getPointeeType().isNull()) {
        qual_type = qual_type->getPointeeType();
    }

    // remove const/volatile
    qual_type = qual_type.getLocalUnqualifiedType();


    if (auto function_type = dyn_cast<FunctionType>(&*qual_type)) {
        std::cerr << fmt::format("treating as function") << std::endl;

        // the type int(bool) from std::function<int(bool)> is a FunctionProtoType
        if (auto function_prototype = dyn_cast<FunctionProtoType>(function_type)) {
            cerr << "IS A FUNCTION PROTOTYPE" << endl;


            cerr << "Recursing on return type" << endl;
            string result = fmt::format("{}(", substitute_type(function_prototype->getReturnType(), template_types));

            bool first_parameter = true;
            for ( auto param : function_prototype->param_types()) {
                if (!first_parameter) {
                    result += ", ";
                }
                first_parameter = false;

                cerr << "Recursing on param type" << endl;
                result += substitute_type(param, template_types);
            }

            result += ")";
            std::cerr << fmt::format("returning substituted function type: {}", result) << std::endl;
            return result;
        } else {
            cerr << "IS NOT A FUNCTION PROTOTYPE" << endl;
        }

    } else {
        cerr << "is not a FUNCTION TYPE" << endl;
    }


    if (auto template_specialization_type = dyn_cast<TemplateSpecializationType>(&*qual_type)) {
        std::cerr << fmt::format("is template specialization type: {}", template_specialization_type->getTemplateName().getAsTemplateDecl()->getNameAsString()) << std::endl;


        auto template_decl = template_specialization_type->getTemplateName().getAsTemplateDecl();

        // named_decl
        auto templated_decl = template_decl->getTemplatedDecl();
        string result = fmt::format("{}<", templated_decl->getQualifiedNameAsString());


        std::cerr << fmt::format("CANONICAL NAME FOR DECL: {}", templated_decl->getQualifiedNameAsString()) << std::endl;

        // go through the template args
        bool first_arg = true;
        for (int i = 0; i < template_specialization_type->getNumArgs(); i++) {
            if (!first_arg) {
                result += ", ";
            }
            first_arg = false;


            auto & arg = template_specialization_type->getArg(i);

            // this code only cares about types, so skip non-type template arguments
            if (arg.getKind() != clang::TemplateArgument::Type) {
                continue;
            }
            auto template_arg_qual_type = arg.getAsType();
            if (template_arg_qual_type.isNull()) {
                if (print_logging) cerr << "qual type is null" << endl;
                continue;
            }
            if (print_logging) {
                cerr << "Recursing on templated type " << template_arg_qual_type.getAsString() << endl;
            }
            result += substitute_type(template_arg_qual_type, template_types);
        }
        result += fmt::format(">");
        std::cerr << fmt::format("returning substituted type: {}", result) << std::endl;
        return result;
    } else {
        if (print_logging) cerr << "Not a template specializaiton type " << qual_type.getAsString() << endl;
    }

//********************************************************************************



    std::string new_type_string = get_type_string(qual_type) + suffix;

    std::cerr << fmt::format("returning: {}", new_type_string) << std::endl;
    return new_type_string;

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
                                             string const & annotation) {
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

vector<string> generate_variable_names(vector<QualType> qual_types, bool with_std_move) {
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







map<string, int> template_instantiations;

// called for handling matches from the matcher

// contains the matchers



