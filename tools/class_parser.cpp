

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
How to run over complete code base using cmake + cotire
add_library(api-gen-template OBJECT ${YOUR_SOURCE_FILES})
target_compile_options(api-gen-template
        PRIVATE -Xclang -ast-dump -fdiagnostics-color=never <== this isn't right yet, this just dumps the ast
        )
set_target_properties(api-gen-template PROPERTIES COTIRE_UNITY_TARGET_NAME "api-gen")
cotire(api-gen-template)
 */

#include <vector>
#include <string>
using namespace std;

//////////////////////////////
// CUSTOMIZE THESE VARIABLES
//////////////////////////////

// if this is defined, only template info will be printed
//#define TEMPLATE_INFO_ONLY

// Having this too high can lead to VERY memory-intensive compilation units
// Single classes (+base classes) with more than this number of declarations will still be in one file.
// TODO: This should be a command line parameter to the plugin
#define MAX_DECLARATIONS_PER_FILE 20

// Any base types you want to always ignore (this may have default values for a project I'm working on)
vector<string> base_types_to_ignore = {"Subscriber"};


vector<string> includes_for_every_class_wrapper_file = {"\"js_casts.h\"",  "<v8toolkit/bidirectional.h>"};

// sometimes files sneak in that just shouldn't be
vector<string> never_include_for_any_file = {"\"v8helpers.h\""};

#include <iostream>
#include <fstream>
#include <sstream>
#include <set>
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

int classes_wrapped = 0;
int methods_wrapped = 0;

namespace {

    int print_logging = 0;

    std::string get_canonical_name_for_decl(const CXXRecordDecl * decl) {
	return decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString();
    }


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
        for (auto base : decl->bases()) {
            if (!is_good_record_decl(base.getType()->getAsCXXRecordDecl())) {
                return false;
            }
        }
        return true;
    }


    // Finds where file_id is included, how it was included, and returns the string to duplicate
    //   that inclusion
    std::string get_include_string_for_fileid(SourceManager & source_manager, FileID & file_id) {
        auto include_source_location = source_manager.getIncludeLoc(file_id);

        // If it's in the "root" file (file id 1), then there's no include for it
        if (include_source_location.isValid()) {
            auto header_file = include_source_location.printToString(source_manager);
//            if (print_logging) cerr << "include source location: " << header_file << endl;
            //            wrapped_class.include_files.insert(header_file);
        } else {
//            if (print_logging) cerr << "No valid source location" << endl;
            return "";
        }

        bool invalid;
        // This gets EVERYTHING after the start of the filename in the include.  "asdf.h"..... or <asdf.h>.....
        const char * text = source_manager.getCharacterData(include_source_location, &invalid);
        const char * text_end = text + 1;
        while(*text_end != '>' && *text_end != '"') {
            text_end++;
        }

        return string(text, (text_end - text) + 1);

    }


    std::string get_include_for_record_decl(SourceManager & source_manager, const CXXRecordDecl * record_decl) {
        if (record_decl == nullptr) {
            return "";
        }
        auto full_source_loc = FullSourceLoc(record_decl->getLocStart(), source_manager);

        auto file_id = full_source_loc.getFileID();
        return get_include_string_for_fileid(source_manager, file_id);
    }


//
//    std::string decl2str(const clang::Decl *d, SourceManager &sm) {
//        // (T, U) => "T,,"
//        std::string text = Lexer::getSourceText(CharSourceRange::getTokenRange(d->getSourceRange()), sm, LangOptions(), 0);
//        if (text.at(text.size()-1) == ',')
//            return Lexer::getSourceText(CharSourceRange::getCharRange(d->getSourceRange()), sm, LangOptions(), 0);
//        return text;
//    }
#if 0

    std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range) {
        std::string text = Lexer::getSourceText(CharSourceRange::getTokenRange(source_range), sm, LangOptions(), 0);
        if (text.at(text.size()-1) == ',')
            return Lexer::getSourceText(CharSourceRange::getCharRange(source_range), sm, LangOptions(), 0);
        return text;
    }


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

    struct WrappedClass {
	CXXRecordDecl const * decl = nullptr;
        string class_name;
        set<string> include_files;
        int declaration_count = 0;
        set<string> methods;
        set<string> members;
        set<string> constructors;
        set<string> names;
        set<WrappedClass *> derived_types;
        set<WrappedClass *> base_types;
        set<string> wrapper_extension_methods;
        SourceManager & source_manager;
	string my_include; // the include for getting my type

	std::string get_short_name() {
	    return decl->getNameAsString();
	}


	std::string make_sfinae_to_match_wrapped_class() {
	    return fmt::format("std::is_same<{}>::value", class_name)
	}
	
        bool ready_for_wrapping(set<WrappedClass *> wrapped_classes) {

            // don't double wrap yourself
            if (find(wrapped_classes.begin(), wrapped_classes.end(), this) != wrapped_classes.end()) {
		//                printf("Already wrapped %s\n", class_name.c_str());
                return false;
            }

	    /*
            // if all this class's directly derived types have been wrapped, then we're good since their
            //   dependencies would have to be met for them to be wrapped
            for (auto derived_type : derived_types) {
                if (find(wrapped_classes.begin(), wrapped_classes.end(), derived_type) == wrapped_classes.end()) {
                    printf("Couldn't find %s\n", derived_type->class_name.c_str());
                    return false;
                }
            }
	    */
            for (auto base_type : base_types) {
                if (find(wrapped_classes.begin(), wrapped_classes.end(), base_type) == wrapped_classes.end()) {
		    //                    printf("base type %s not already wrapped - return false\n", base_type->class_name.c_str());
                    return false;
                }
            }

	    //            printf("Ready to wrap %s\n", class_name.c_str());

            return true;
        }

        std::set<string> get_derived_type_includes() {
            set<string> results;
	    results.insert(my_include);
            for (auto derived_type : derived_types) {
                auto derived_includes = derived_type->get_derived_type_includes();
                results.insert(derived_includes.begin(), derived_includes.end());
            }
            return results;
        }

        WrappedClass(const CXXRecordDecl * decl, const std::string & class_name, SourceManager & source_manager, string my_include) :
	    decl(decl), class_name(class_name), source_manager(source_manager), my_include(my_include)
        {}

        std::string get_derived_classes_string(int level = 0, const std::string indent = ""){
            vector<string> results;
	    //            printf("%s In (%d) %s looking at %d derived classes\n", indent.c_str(), level, class_name.c_str(), (int)derived_types.size());
            for (WrappedClass * derived_class : derived_types) {
                results.push_back(derived_class->class_name);
                results.push_back(derived_class->get_derived_classes_string(level + 1, indent + "  "));
            }
	    //            printf("%s Returning %s\n", indent.c_str(), join(results).c_str());
            return join(results);
        }

        std::string get_base_class_string(){
            auto i = base_types.begin();
            while(i != base_types.end()) {
                if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), (*i)->class_name) !=
                    base_types_to_ignore.end()) {
                    base_types.erase(i++); // move iterator before erasing
                } else {
                    i++;
                }
            };
            if (base_types.size() > 1) {
		llvm::report_fatal_error(fmt::format("Type {} has more than one base class - this isn't supported because javascript doesn't support MI\n", class_name), false);

            }
            return base_types.size() ? (*base_types.begin())->class_name : "";
        }

        std::string get_wrapper_string(){
            stringstream result;
            string indentation = "  ";

            result << indentation << "{\n";
            result << fmt::format("{}  // {}", indentation, class_name) << "\n";
            result << fmt::format("{}  v8toolkit::V8ClassWrapper<{}> & class_wrapper = isolate.wrap_class<{}>();\n",
                                  indentation, class_name, class_name);
            result << fmt::format("{}  class_wrapper.set_class_name(\"{}\");\n", indentation, class_name);

            for(auto & method : methods) {
                result << method;
            }
            for(auto & member : members) {
                result << member;
            }
            for(auto & wrapper_extension_method : wrapper_extension_methods) {
                result << fmt::format("{}  {}\n", indentation, wrapper_extension_method);
            }
            if (!derived_types.empty()) {
                result << fmt::format("{}  class_wrapper.set_compatible_types<{}>();\n", indentation,
                                      get_derived_classes_string());
            }
            if (get_base_class_string() != "") {
                result << fmt::format("{}  class_wrapper.set_parent_type<{}>();\n", indentation,
                                      get_base_class_string());
            }
            result << fmt::format("{}  class_wrapper.finalize();\n", indentation);

            for(auto & constructor : constructors) {
                result << constructor;
            }

            result << indentation << "}\n\n";
            return result.str();
        }

    };



    string get_sfinae_matching_wrapped_classes() {
	vector<string> sfinaes;
	for (auto & wrapped_class : wrapped_classes) {
	    sfinaes.emplace_back(wrapped_class.make_sfinae_to_match_wrapped_class);
	}
	return join(sfinaes);
    }
    

    // returns a vector of all the annotations on a Decl
    std::vector<std::string> get_annotations(const Decl * decl) {
        std::vector<std::string> results;
        for (auto attr : decl->getAttrs()) {
            AnnotateAttr * annotation =  dyn_cast<AnnotateAttr>(attr);
            if (annotation) {
                auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
                auto annotation_string = attribute_attr->getAnnotation().str();
                //if (print_logging) cerr << "Got annotation " << annotation_string << endl;
                results.emplace_back(annotation->getAnnotation().str());
            }
        }
        return results;
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





    bool has_annotation(const Decl * decl, const std::string & target) {
        auto annotations = get_annotations(decl);
        return std::find(annotations.begin(), annotations.end(), target) != annotations.end();
    }


    enum EXPORT_TYPE {
        EXPORT_UNSPECIFIED = 0,
        EXPORT_NONE, // export nothing
        EXPORT_SOME, // only exports specifically marked entities
        EXPORT_EXCEPT, // exports everything except specifically marked entities
        EXPORT_ALL}; // exports everything

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
    std::string handle_std(const std::string & input) {
        smatch matches;
	// EricWF said to remove __[0-9] just to be safe for future updates
        regex_match(input, matches, regex("^((?:const\\s+|volatile\\s+)*)(?:class |struct )?(?:std::(?:__[0-9]::)?)?(.*)"));
        // space before std:: is handled from const/volatile if needed
        auto result = matches[1].str() + "std::" + matches[2].str();

        if (print_logging) cerr << "Stripping std from " << input << " results in " << result << endl;
        return result;
    }

    bool has_std(const std::string & input) {
        return std::regex_match(input, regex("^(const\\s+|volatile\\s+)*(class |struct )?\\s*std::.*$"));
    }



    // Returns true if qual_type is a 'trivial' std:: type like
    //   std::string
    bool is_trivial_std_type(QualType & qual_type, std::string & output) {
        std::string name = qual_type.getAsString();
        std::string canonical_name = qual_type.getCanonicalType().getAsString();

        // if it's a std:: type and not explicitly user-specialized, pass it through
        if (std::regex_match(name, regex("^(const\\s+|volatile\\s+)*(class |struct )?std::[^<]*$"))) {
            output = handle_std(name);
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
                cerr << "ERROR: detected template parameters > actual list size" << endl;
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
    void update_wrapped_class_for_type(SourceManager & source_manager,
                                       WrappedClass & wrapped_class,
                                       // don't capture qualtype by ref since it is changed in this function
                                       QualType qual_type) {

        if (print_logging) cerr << "Went from " << qual_type.getAsString();
        qual_type = qual_type.getLocalUnqualifiedType();

        while(!qual_type->getPointeeType().isNull()) {
            qual_type = qual_type->getPointeeType();
        }
        qual_type = qual_type.getLocalUnqualifiedType();

        if (print_logging) cerr << " to " << qual_type.getAsString() << endl;
        auto base_type_record_decl = qual_type->getAsCXXRecordDecl();

       // primitive types don't have record decls
        if (base_type_record_decl == nullptr) {
            return;
        }

	auto actual_include_string = get_include_for_record_decl(source_manager, base_type_record_decl);

        if (print_logging) cerr << &wrapped_class << "Got include string for " << qual_type.getAsString() << ": " << actual_include_string << endl;
	
        wrapped_class.include_files.insert(actual_include_string);

        if (dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl)) {
            if (print_logging) cerr << "##!#!#!#!# Oh shit, it's a template type " << qual_type.getAsString() << endl;

            auto template_specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl);

            auto & template_arg_list = template_specialization_decl->getTemplateArgs();
            for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
                auto & arg = template_arg_list[i];

                // this code only cares about types
                if (arg.getKind() != clang::TemplateArgument::Type) {
                    continue;
                }
                auto template_arg_qual_type = arg.getAsType();
                if (template_arg_qual_type.isNull()) {
                    if (print_logging) cerr << "qual type is null" << endl;
                    continue;
                }
		if (print_logging) cerr << "Recursing on templated type " << template_arg_qual_type.getAsString() << endl;
                update_wrapped_class_for_type(source_manager, wrapped_class, template_arg_qual_type);
            }
        } else {
            if (print_logging) cerr << "Not a template specializaiton type " << qual_type.getAsString() << endl;
        }
    }


    vector<QualType> get_method_param_qual_types(const CXXMethodDecl * method,
                                                 const string & annotation = "") {
        vector<QualType> results;
        auto parameter_count = method->getNumParams();
        for (unsigned int i = 0; i < parameter_count; i++) {
            auto param_decl = method->getParamDecl(i);
            if (annotation != "" && !has_annotation(param_decl, annotation)) {
                if (print_logging) cerr << "Skipping method parameter because it didn't have requested annotation: " << annotation << endl;
                continue;
            }
            auto param_qual_type = param_decl->getType();
            results.push_back(param_qual_type);
        }
        return results;
    }

    vector<string> generate_variable_names(std::size_t count) {
        vector<string> results;
        for (std::size_t i = 0; i < count; i++) {
            results.push_back(fmt::format("var{}", i+1));
        }
        return results;
    }

    std::string get_method_parameters(SourceManager & source_manager,
                                      WrappedClass & wrapped_class,
                                      const CXXMethodDecl * method,
                                      bool add_leading_comma = false,
                                      bool insert_variable_names = false,
                                      const string & annotation = "") {
        std::stringstream result;
        bool first_param = true;
        auto type_list = get_method_param_qual_types(method, annotation);

        if (!type_list.empty() && add_leading_comma) {
            result << ", ";
        }
        int count = 0;
        auto var_names = generate_variable_names(type_list.size());
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
            update_wrapped_class_for_type(source_manager, wrapped_class, param_qual_type);

        }
        return result.str();
    }

    std::string get_return_type(SourceManager & source_manager,
                                      WrappedClass & wrapped_class,
                                      const CXXMethodDecl * method) {
        auto qual_type = method->getReturnType();
        auto result = get_type_string(qual_type);
//        auto return_type_decl = qual_type->getAsCXXRecordDecl();
//        auto full_source_loc = FullSourceLoc(return_type_decl->getLocStart(), source_manager);
//        auto header_file = strip_path_from_filename(source_manager.getFilename(full_source_loc).str());
//        if (print_logging) cerr << fmt::format("{} needs {}", wrapped_class.class_name, header_file) << endl;
//        wrapped_class.include_files.insert(header_file);
//

        update_wrapped_class_for_type(source_manager, wrapped_class, qual_type);

        return result;

    }


    std::string get_method_return_type_class_and_parameters(SourceManager & source_manager,
                                                            WrappedClass & wrapped_class,
                                                            const CXXRecordDecl * klass, const CXXMethodDecl * method) {
        std::stringstream results;
        results << get_return_type(source_manager, wrapped_class, method);
        results << ", " << get_canonical_name_for_decl(klass);
        results << get_method_parameters(source_manager, wrapped_class, method, true);
        return results.str();
    }

    std::string get_method_return_type_and_parameters(SourceManager & source_manager,
                                                      WrappedClass & wrapped_class,
                                                      const CXXRecordDecl * klass, const CXXMethodDecl * method) {
        std::stringstream results;
        results << get_return_type(source_manager, wrapped_class, method);
        results << get_method_parameters(source_manager, wrapped_class, method, true);
        return results.str();
    }




    std::string get_method_string(SourceManager & source_manager,
                                  WrappedClass & wrapped_class,
                                  const CXXMethodDecl * method) {
        std::stringstream result;
        result << method->getReturnType().getAsString();

        result << method->getName().str();

        result << "(";

        result << get_method_parameters(source_manager, wrapped_class, method);

        result << ")";

        return result.str();
    }






    // calls callback for each constructor in the class.  If annotation specified, only
    //   constructors with that annotation will be sent to the callback
    template<class Callback>
    void foreach_constructor(const CXXRecordDecl * klass, Callback && callback,
                             const std::string & annotation = "") {
	//bool print_logging = true;

	if (print_logging) cerr << "Enumerating constructors for " << klass->getNameAsString() << " with optional annotation: " << annotation << endl;
	
        for(CXXMethodDecl * method : klass->methods()) {
            CXXConstructorDecl * constructor = dyn_cast<CXXConstructorDecl>(method);
	    bool skip = false;

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

            if (annotation != "" && !has_annotation(constructor, annotation)) {
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
        SourceManager & source_manager;
        const CXXRecordDecl * starting_class;
        WrappedClass & wrapped_class;

    public:
        BidirectionalBindings(SourceManager & source_manager,
                              const CXXRecordDecl * starting_class,
                              WrappedClass & wrapped_class) :
                source_manager(source_manager),
                starting_class(starting_class),
                wrapped_class(wrapped_class) {}

        std::string short_name(){return starting_class->getName();}
	std::string canonical_name(){return get_canonical_name_for_decl(starting_class);}

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
                            if(get_method_string(source_manager, wrapped_class, method) ==
                                    get_method_string(source_manager, wrapped_class, found)) {
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
                auto types = get_method_param_qual_types(method);
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
            auto annotations = get_annotations(starting_class);
            auto matches = get_annotation_regex(starting_class, "v8toolkit_generate_(.*)");
            if (has_annotation(starting_class, std::string(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING))) {
                result << fmt::format("class JS{} : public {}, public v8toolkit::JSWrapper<{}> {{\npublic:\n", // {{ is escaped {
                                      short_name(), short_name(), short_name());
                result << fmt::format("    JS{}(v8::Local<v8::Context> context, v8::Local<v8::Object> object,\n", short_name());
                result << fmt::format("        v8::Local<v8::FunctionTemplate> created_by");
                bool got_constructor = false;
                int constructor_parameter_count;
                foreach_constructor(starting_class, [&](auto constructor_decl){
                    if (got_constructor) { cerr << "ERROR: Got more than one constructor" << endl; return;}
                    got_constructor = true;
                    result << get_method_parameters(source_manager, wrapped_class, constructor_decl, true, true);
                    constructor_parameter_count = constructor_decl->getNumParams();

                }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);
                if (!got_constructor) {
		    llvm::report_fatal_error(fmt::format("ERROR: Got no bidirectional constructor for {}", starting_class->getNameAsString()), false);

                }
                result << fmt::format(") :\n");

                auto variable_names = generate_variable_names(constructor_parameter_count);

                result << fmt::format("      {}({}),\n", short_name(), join(variable_names));
                result << fmt::format("      v8toolkit::JSWrapper<{}>(context, object, created_by) {{}}\n", short_name()); // {{}} is escaped {}
                result << handle_class(starting_class);
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

	    // find corresponding wrapped class
	    const WrappedClass * this_class = nullptr;
	    for (auto & wrapped_class : wrapped_classes) {
		if (wrapped_class->class_name == canonical_name()) {
		    this_class = wrapped_class.get();
		    break;
		}
	    }
	    
	    if (this_class == nullptr) {
		llvm::report_fatal_error(fmt::format("couldn't find wrapped class for {}", short_name()), false);
	    }

	    // This needs include files because the IMPLEMENTATION goes in the file.  If it were moved out, then the header file could
	    //   rely soley on the primary type's includes 
	    for (auto & include : this_class->include_files) {
	        if (include == ""){continue;}
		bidirectional_class_file << "#include " << include << "\n";
	    }
	    
            bidirectional_class_file << result.str();
            bidirectional_class_file.close();


        }
    };






    map<string, int> template_instantiations;

    class ClassHandler : public MatchFinder::MatchCallback {
    private:


//        CompilerInstance &CI;

        SourceManager & source_manager;
        std::vector<unique_ptr<WrappedClass>> & wrapped_classes;
        WrappedClass * get_or_insert_wrapped_class(const CXXRecordDecl * decl) {
            //fprintf(stderr, "get or insert wrapped class %p\n", (void*)decl);

            auto class_name = get_canonical_name_for_decl(decl);
            //fprintf(stderr, " -- class name %s\n", class_name.c_str());
            for (auto & wrapped_class : wrapped_classes) {
                if (wrapped_class->class_name == class_name) {
                    return wrapped_class.get();
                }
            }
            wrapped_classes.emplace_back(std::make_unique<WrappedClass>(decl, get_canonical_name_for_decl(decl), source_manager, get_include_for_record_decl(source_manager, decl)));
            return wrapped_classes.back().get();
        }

        WrappedClass * current_wrapped_class; // the class currently being wrapped
        std::set<std::string> names_used;
        const CXXRecordDecl * top_level_class_decl = nullptr;

    public:




        ClassHandler(CompilerInstance &CI,
                     std::vector<unique_ptr<WrappedClass>> & wrapped_classes) :
            source_manager(CI.getSourceManager()),
            wrapped_classes(wrapped_classes)
        {}



        std::string handle_data_member(const CXXRecordDecl * containing_class, FieldDecl * field, EXPORT_TYPE parent_export_type, const std::string & indentation) {
            std::stringstream result;
            auto export_type = get_export_type(field, parent_export_type);
            auto short_field_name = field->getNameAsString();
            auto full_field_name = field->getQualifiedNameAsString();

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

            if (current_wrapped_class->names.count(short_field_name)) {
                printf("WARNING: Skipping duplicate name %s/%s :: %s\n",
                       top_level_class_decl->getName().str().c_str(),
                        containing_class->getName().str().c_str(),
                        short_field_name.c_str());
                return "";
            }
            current_wrapped_class->names.insert(short_field_name);


            current_wrapped_class->declaration_count++;

            update_wrapped_class_for_type(source_manager, *current_wrapped_class, field->getType());

            if (has_annotation(field, V8TOOLKIT_READONLY_STRING)) {
                result << fmt::format("{}class_wrapper.add_member_readonly(\"{}\", &{});\n", indentation,
                                      short_field_name, full_field_name);
            } else {
                result << fmt::format("{}class_wrapper.add_member(\"{}\", &{});\n", indentation,
                                      short_field_name, full_field_name);
            }
//            printf("%sData member %s, type: %s\n",
//                   indentation.c_str(),
//                   field->getNameAsString().c_str(),
//                   field->getType().getAsString().c_str());
            return result.str();
        }


        std::string handle_method(const CXXRecordDecl * containing_class, CXXMethodDecl * method, EXPORT_TYPE parent_export_type, const std::string & indentation) {

            std::stringstream result;

            std::string full_method_name(method->getQualifiedNameAsString());
            std::string short_method_name(method->getNameAsString());

//            if (print_logging) cerr << "changing method name from " << full_method_name << " to ";
//
//            auto regex = std::regex(fmt::format("{}::{}$", containing_class->getName().str(), short_method_name));
//            auto replacement = fmt::format("{}::{}", top_level_class_decl->getName().str(), short_method_name);
//            full_method_name = std::regex_replace(full_method_name, regex, replacement);
//            if (print_logging) cerr << full_method_name << endl;


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
                if(!method->isVirtual()) {
		    llvm::report_fatal_error("Got pure non-virtual method - not sure what that even means", false);
		}
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), full_method_name.c_str());
                return "";
            }
            if (dyn_cast<CXXConversionDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("{}**skipping user-defined conversion operator", indentation) << endl;
                return "";
            }

            // If this
            if (has_annotation(method, V8TOOLKIT_EXTEND_WRAPPER_STRING)) {
                if (!method->isStatic()) {
		    llvm::report_fatal_error(fmt::format("method {} annotated with V8TOOLKIT_EXTEND_WRAPPER must be static", full_method_name.c_str()), false);

                }
                if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("{}**skipping static method marked as v8 class wrapper extension method, but will call it during class wrapping", indentation) << endl;
                current_wrapped_class->wrapper_extension_methods.insert(full_method_name + "(class_wrapper);");

                return "";
            }

            if (current_wrapped_class->names.count(short_method_name)) {
                printf("Skipping duplicate name %s/%s :: %s\n",
                       top_level_class_decl->getName().str().c_str(),
                       containing_class->getName().str().c_str(),
                       short_method_name.c_str());
                return "";
            }
            current_wrapped_class->names.insert(short_method_name);



            result << indentation;



            if (method->isStatic()) {
                current_wrapped_class->declaration_count++;
                result << fmt::format("class_wrapper.add_static_method<{}>(\"{}\", &{});\n",
                       get_method_return_type_and_parameters(source_manager, *current_wrapped_class, containing_class, method),
                       short_method_name, full_method_name);
            } else {
                current_wrapped_class->declaration_count++;
                result << fmt::format("class_wrapper.add_method<{}>(\"{}\", &{});\n",
                       get_method_return_type_class_and_parameters(source_manager, *current_wrapped_class, containing_class, method),
                       short_method_name, full_method_name);
                methods_wrapped++;

            }
            return result.str();
        }


        void handle_class(const CXXRecordDecl * klass,
                          EXPORT_TYPE parent_export_type = EXPORT_UNSPECIFIED,
                          bool top_level = true,
                          WrappedClass * derived_class = nullptr,
                          const std::string & indentation = "") {

            if (top_level) {
                cerr << "**** In top-level handle_class with type: " << get_canonical_name_for_decl(klass) << endl;

		if (dyn_cast<ClassTemplatePartialSpecializationDecl>(klass)) {
		    cerr << "is class template partial specilziation decl" << endl;
		}
		if (dyn_cast<ClassTemplateSpecializationDecl>(klass)) {
		    cerr << "is class template specialization decl" << endl;
		}
	    

		
		if (klass->getTypeForDecl()->isDependentType()) {
		    if (print_logging) cerr << "Skipping dependent type top-level class" << endl;
		    return;
		}
                const ClassTemplateSpecializationDecl * specialization = nullptr;
                if ((specialization = dyn_cast<ClassTemplateSpecializationDecl>(klass)) != nullptr) {
		    auto specialized_template = specialization->getSpecializedTemplate();
		    auto template_name = specialized_template->getNameAsString();
		    if (template_name == "remove_reference") {
			cerr << get_canonical_name_for_decl(klass) << endl;
		    }
		    template_instantiations[template_name]++;
		}
	       

#ifdef TEMPLATE_INFO_ONLY
		return;
#endif
                if (!is_good_record_decl(klass)) {
                    if (print_logging) cerr << "Skipping 'bad' CXXRecordDecl" << endl;
                    return;
                }


                classes_wrapped++;
                names_used.clear();
                current_wrapped_class = get_or_insert_wrapped_class(klass);

		//                printf("Handling top level class %s\n", current_wrapped_class->class_name.c_str());


                if (print_logging) cerr << "*&&&&&&&&&&&&&&&adding include for class being handled: " << klass->getName().str() << " : " << get_include_for_record_decl(source_manager, klass) << endl;
                current_wrapped_class->include_files.insert(get_include_for_record_decl(source_manager, klass));

                // if this is a bidirectional class, make a minimal wrapper for it
                if (has_annotation(klass, V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {
                    if (print_logging) cerr << "Type " << current_wrapped_class->class_name << " **IS** bidirectional" << endl;

		    auto generated_header_name = fmt::format("\"v8toolkit_generated_bidirectional_{}.h\"", current_wrapped_class->get_short_name());
		    
                    auto bidirectional_class_name = fmt::format("JS{}", current_wrapped_class->get_short_name());
                    auto bidirectional_unique_ptr = std::make_unique<WrappedClass>((CXXRecordDecl*)nullptr, bidirectional_class_name, source_manager, generated_header_name);
                    auto & bidirectional = *bidirectional_unique_ptr;
                    bidirectional.base_types.insert(current_wrapped_class);
                    current_wrapped_class->derived_types.insert(&bidirectional);
                    bidirectional.include_files.insert(generated_header_name);

                    wrapped_classes.emplace_back(move(bidirectional_unique_ptr));

                } else {
                    if (print_logging) cerr << "Type " << current_wrapped_class->class_name << " is not bidirectional" << endl;
                }
            } else {
                cerr << fmt::format("{} Handling class (NOT top level) {}", indentation, klass->getNameAsString()) << endl;
            }



            auto class_name = klass->getQualifiedNameAsString();
            auto export_type = get_export_type(klass, parent_export_type);
            if (export_type == EXPORT_NONE || export_type == EXPORT_UNSPECIFIED) {
                if (PRINT_SKIPPED_EXPORT_REASONS) fprintf(stderr,"%sSkipping class %s marked EXPORT_NONE\n", indentation.c_str(), class_name.c_str());
                return;
            }

            // prints out source for decl
            //fprintf(stderr,"class at %s", decl2str(klass,  source_manager).c_str());

            auto full_source_loc = FullSourceLoc(klass->getLocation(), source_manager);
            auto file_id = full_source_loc.getFileID();

//            fprintf(stderr,"%sClass/struct: %s\n", indentation.c_str(), class_name.c_str());
            // don't do the following code for inherited classes
            if (top_level){
                current_wrapped_class->include_files.insert(get_include_string_for_fileid(source_manager, file_id));
            }
//            fprintf(stderr,"%s Decl at line %d, file id: %d %s\n", indentation.c_str(), full_source_loc.getExpansionLineNumber(),
//                   full_source_loc.getFileID().getHashValue(), source_manager.getBufferName(full_source_loc));

//                auto type_decl = dyn_cast<TypeDecl>(klass);
//                assert(type_decl);
//                auto type = type_decl->getTypeForDecl();

//
            for(CXXMethodDecl * method : klass->methods()) {
                current_wrapped_class->methods.insert(handle_method(klass, method, export_type, indentation + "  "));
            }

            for (FieldDecl * field : klass->fields()) {
                current_wrapped_class->members.insert(handle_data_member(klass, field, export_type, indentation + "  "));
            }

	    // if this is true and the type ends up with no base type, it's an error
	    bool must_have_base_type = false;
            auto base_types_to_ignore = get_annotation_regex(klass, "^" V8TOOLKIT_IGNORE_BASE_TYPE_PREFIX "(.*)$");
            auto base_type_to_use = get_annotation_regex(klass, "^" V8TOOLKIT_USE_BASE_TYPE_PREFIX "(.*)$");
            if (base_type_to_use.size() > 1) {
                llvm::report_fatal_error("More than one base type specified to use for type");
            }

	    // if a base type to use is specified, then it must match an actual base type or error
	    if (!base_type_to_use.empty()) {
		must_have_base_type = true;
	    }


	    print_vector(base_types_to_ignore, "base types to ignore");
	    print_vector(base_type_to_use, "base type to use");


	    bool found_base_type = false;
            for (auto base_class : klass->bases()) {

                auto base_qual_type = base_class.getType();
		auto base_type_decl = base_qual_type->getAsCXXRecordDecl();
		auto base_type_name = base_type_decl->getNameAsString();

		
		cerr << "Base type: " << base_qual_type.getAsString() <<  endl;
                if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), base_type_name) !=
                        base_types_to_ignore.end()) {
                    cerr << "Skipping base type because it was explicitly excluded via annotation: " << base_type_name << endl;
                    continue;
                }
                if (!base_type_to_use.empty() && base_type_to_use[0] != base_type_name) {
                    cerr << "Skipping base type because it was not the one specified to use via annotation: " << base_type_name << endl;
                    continue;
                }

                if (base_qual_type->isDependentType()) {
                    cerr << indentation << "-- base type is dependent" << endl;
                }



		found_base_type = true;
                auto record_decl = base_qual_type->getAsCXXRecordDecl();

//                fprintf(stderr, "%s -- type class: %d\n", indentation.c_str(), base_qual_type->getTypeClass());
//                cerr << indentation << "-- base type has a cxxrecorddecl" << (record_decl != nullptr) << endl;
//                cerr << indentation << "-- base type has a tagdecl: " << (base_tag_decl != nullptr) << endl;
//                cerr << indentation << "-- can be cast to tagtype: " << (dyn_cast<TagType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to attributed type: " << (dyn_cast<AttributedType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to injected class name type: " << (dyn_cast<InjectedClassNameType>(base_qual_type) != nullptr) << endl;

                // This is ok.  Not sure why.
                if (record_decl == nullptr) {
                    llvm::report_fatal_error("Got null base type record decl - this should be caught ealier");
                }
		//  printf("Found parent/base class %s\n", record_decl->getNameAsString().c_str());

                //cerr << "getting derived type wrapped class object" << endl;
                WrappedClass * current = get_or_insert_wrapped_class(klass);

                //cerr << "getting base type wrapped class object" << endl;
                WrappedClass * current_base = get_or_insert_wrapped_class(record_decl);

                auto current_base_include = get_include_for_record_decl(source_manager, current_base->decl);
                auto current_include = get_include_for_record_decl(source_manager, current->decl);
		//                printf("For %s, include %s -- for %s, include %s\n", current_base->class_name.c_str(), current_base_include.c_str(), current->class_name.c_str(), current_include.c_str());

                current->include_files.insert(current_base_include);
                current_base->include_files.insert(current_include);
                current->base_types.insert(current_base);
                current_base->derived_types.insert(current);

                //printf("%s now has %d base classes\n", current->class_name.c_str(), (int)current->base_types.size());
                //printf("%s now has %d derived classes\n", current_base->class_name.c_str(), (int)current_base->derived_types.size());

                handle_class(record_decl, export_type, false, current_wrapped_class, indentation + "  ");
            }

	    if (must_have_base_type && !found_base_type) {
		llvm::report_fatal_error("base_type_to_use specified but no base type found");
	    }

            std::vector<std::string> used_constructor_names;

            if (top_level) {
                if (klass->isAbstract()) {
//                    if (print_logging) cerr << "Skipping all constructors because class is abstract: " << class_name << endl;
                } else {
                    foreach_constructor(klass, [&](auto constructor) {

//                        auto full_source_loc = FullSourceLoc(constructor->getLocation(), source_manager);
//                        fprintf(stderr,"%s %s constructor Decl at line %d, file id: %d %s\n", indentation.c_str(),
//                                top_level_class_decl->getName().str().c_str(),
//                                full_source_loc.getExpansionLineNumber(),
//                                full_source_loc.getFileID().getHashValue(),
//                                source_manager.getBufferName(full_source_loc));


                        if (constructor->isCopyConstructor()) {
//                            fprintf(stderr,"Skipping copy constructor\n");
                            return;
                        } else if (constructor->isMoveConstructor()) {
//                            fprintf(stderr,"Skipping move constructor\n");
                            return;
                        } else if (constructor->isDeleted()) {
//                            if (print_logging) cerr << "Skipping deleted constructor" << endl;
                            return;
                        }
                        auto annotations = get_annotation_regex(constructor, V8TOOLKIT_CONSTRUCTOR_PREFIX "(.*)");
//                        fprintf(stderr,"Got %d annotations on constructor\n", (int)annotations.size());
                        std::string constructor_name = class_name;
                        if (!annotations.empty()) {
                            constructor_name = annotations[0];
                        }
                        if (std::find(used_constructor_names.begin(), used_constructor_names.end(), constructor_name) !=
                            used_constructor_names.end()) {
                            cerr << fmt::format("Error: because duplicate JS constructor function name {}",
                                                constructor_name.c_str()) << endl;
                            for (auto &name : used_constructor_names) {
                                if (print_logging) cerr << name << endl;
                            }
                            throw std::exception();
                        }
                        used_constructor_names.push_back(constructor_name);

                        current_wrapped_class->constructors.insert(fmt::format("{}  class_wrapper.add_constructor<{}>(\"{}\", isolate);\n",
                                              indentation, get_method_parameters(source_manager,
                                                                                 *current_wrapped_class,
                                                                                 constructor), constructor_name));
                    });
                }
            }
        }


        /**
         * This runs per-match from MyASTConsumer, but always on the same ClassHandler object
         */
        virtual void run(const MatchFinder::MatchResult &Result) {
            if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class")) {
                //printf("In 'run', with cxxrecorddecl: %p %s\n", (void*)klass, klass->getNameAsString().c_str());
                this->top_level_class_decl = klass;

                handle_class(klass, EXPORT_UNSPECIFIED, true, nullptr, "  ");

#ifndef TEMPLATE_INFO_ONLY
                BidirectionalBindings bidirectional(source_manager, klass, *current_wrapped_class);
                bidirectional.generate_bindings(wrapped_classes);
#endif

            } else if (const CXXMethodDecl * method = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("method")) {
		auto method_name =  method->getQualifiedNameAsString() ;
		//		cerr << "Got method: " << method_name << endl;
		template_instantiations[method_name]++;
	    }
        }
    };





    







    // Implementation of the ASTConsumer interface for reading an AST produced
    // by the Clang parser. It registers a couple of matchers and runs them on
    // the AST.
    class MyASTConsumer : public ASTConsumer {
    public:
        MyASTConsumer(CompilerInstance &CI,
                      std::vector<unique_ptr<WrappedClass>> & wrapped_classes) :
                HandlerForClass(CI, wrapped_classes) {

#ifdef TEMPLATE_INFO_ONLY
	    /*
	    Matcher.addMatcher(cxxRecordDecl(allOf(
                    anyOf(isStruct(), isClass()),
		    allOf(
			  hasAttr(attr::Annotate), // can't check the actual annotation value here
			  isDefinition() // skip forward declaration
			  )
						   )).bind("class"), &HandlerForClass);
	    */
	    Matcher.addMatcher(cxxMethodDecl(
					     isTemplateInstantiation()
					     ).bind("method"),
			       &HandlerForClass);

#else
	    Matcher.addMatcher(cxxRecordDecl(allOf(
                    anyOf(isStruct(), isClass()),
		    allOf(
			  hasAttr(attr::Annotate), // can't check the actual annotation value here
			  isDefinition() // skip forward declaration
			  )
                    )

					     ).bind("class"),
                &HandlerForClass);
		    
#endif

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

        }

        // This is called when all parsing is done
        void EndSourceFileAction() {
            static bool already_called = false;

	    {
		printf("Template instantiations: \n");
		vector<pair<string, int>> insts;
		for (auto & pair : template_instantiations) {
		    insts.push_back(pair);
		}
		std::sort(insts.begin(), insts.end(), [](auto & a, auto & b){
			return a.second < b.second;
		    });
		int skipped = 0;
		int skip_threshold = 10;
		int total = 0;
		for (auto & pair : insts) {
		    total += pair.second;
		    if (pair.second < skip_threshold) {
		    skipped++;
		    continue;
		    }
		    cerr << pair.first << ": " << pair.second << endl;;
		}
		
		cerr << "Skipped " << skipped << " entries because they had fewer than " << skip_threshold << " instantiations" << endl;
		cerr << "Total of " << total << " instantiations" << endl;
	    }

            if (already_called) {
                if (print_logging) cerr << "This plugin doesn't work if there's more than one file.   Use it on a unity build" << endl;
                throw std::exception();
            }
            already_called = true;

            // Write class wrapper data to a file
            int file_count = 1;

            int declaration_count_this_file = 0;
            vector<WrappedClass*> classes_for_this_file;

            set<WrappedClass *> already_wrapped_classes;

            bool found_match = true;
            while (found_match && already_wrapped_classes.size() < wrapped_classes.size()) {
                found_match = false;

                for (auto wrapped_class_iterator = wrapped_classes.begin();
                     wrapped_class_iterator != wrapped_classes.end();
                     wrapped_class_iterator++) {

                    WrappedClass &wrapped_class = **wrapped_class_iterator;

                    // if it has unmet dependencies or has already been mapped, skip it
                    if (!wrapped_class.ready_for_wrapping(already_wrapped_classes)) {
			//                        printf("Skipping %s\n", wrapped_class.class_name.c_str());
                        continue;
                    }
                    already_wrapped_classes.insert(&wrapped_class);
                    found_match = true;


                    if (print_logging) cerr << "dumping wrapped class " << wrapped_class.class_name << endl;
                    // if there's room in the current file, add this class
                    auto space_available = declaration_count_this_file == 0 ||
                                           declaration_count_this_file + wrapped_class.declaration_count <
                                           MAX_DECLARATIONS_PER_FILE;
                    auto last_class = already_wrapped_classes.size() == wrapped_classes.size();

                    if (!space_available) {
                        //printf("Actually writing file to disk\n");
                        write_classes(file_count, classes_for_this_file, last_class);

                        // reset for next file
                        classes_for_this_file.clear();
                        declaration_count_this_file = 0;
                        file_count++;
                    }

                    classes_for_this_file.push_back(&wrapped_class);
                    declaration_count_this_file += wrapped_class.declaration_count;


                    if (!classes_for_this_file.empty() && last_class) {
                        write_classes(file_count++, classes_for_this_file, true);
                    }
                }
            }

            if (already_wrapped_classes.size() != wrapped_classes.size()) {
		llvm::report_fatal_error(fmt::format("Could not wrap all classes - wrapped {} out of {}",
					 already_wrapped_classes.size(), wrapped_classes.size()), false);
            }

            if (print_logging) cerr << "Wrapped " << classes_wrapped << " classes with " << methods_wrapped << " methods" << endl;

        }

        // takes a file number starting at 1 and incrementing 1 each time
        // a list of WrappedClasses to print
        // and whether or not this is the last file to be written
        void write_classes(int file_count, vector<WrappedClass*> & classes, bool last_one) {
            // Open file
            string class_wrapper_filename = fmt::format("v8toolkit_generated_class_wrapper_{}.cpp", file_count);
            ofstream class_wrapper_file;

            class_wrapper_file.open(class_wrapper_filename, ios::out);
            if (!class_wrapper_file) {
                if (print_logging) cerr << "Couldn't open " << class_wrapper_filename << endl;
                throw std::exception();
            }

	    // by putting in some "fake" includes, it will stop these from ever being put in since it will
	    //   think they already are, even though they really aren't
            set<string> already_included_this_file;
	    already_included_this_file.insert(never_include_for_any_file.begin(), never_include_for_any_file.end());

	    // first the ones that go in every file regardless of its contents
	    for (auto & include : includes_for_every_class_wrapper_file) {
		class_wrapper_file << fmt::format("#include {}\n", include);
		already_included_this_file.insert(include);
	    }

            for (WrappedClass * wrapped_class : classes) {
		//                printf("While dumping classes to file, %s has includes: ", wrapped_class->class_name.c_str());

                // Combine the includes needed for types in members/methods with the includes for the wrapped class's
                //   derived types
                auto include_files = wrapped_class->include_files;
                auto base_type_includes = wrapped_class->get_derived_type_includes();
                include_files.insert(base_type_includes.begin(), base_type_includes.end());
		
                for(auto & include_file : include_files) {
		    //  printf("%s ", include_file.c_str());
                    if (include_file != "" && already_included_this_file.count(include_file) == 0) {

                        // skip "internal looking" includes - look at 1 because 0 is < or "
                        if (include_file.find("__") == 1) {
                            continue;
                        }
                        class_wrapper_file << fmt::format("#include {}\n", include_file);
                        already_included_this_file.insert(include_file);
                    }
                }
		//                printf("\n");
            }

            // Write function header
            class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate &); // may not exist -- that's ok\n", file_count+1);
            if (file_count == 1) {
                class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers(v8toolkit::Isolate & isolate) {{\n");

            } else {
                class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate & isolate) {{\n",
                                                  file_count);
            }

            // Print function body
            for (auto wrapped_class : classes) {
                class_wrapper_file << wrapped_class->get_wrapper_string();
            }


            // if there's going to be another file, call the function in it
            if (!last_one) {
                class_wrapper_file << fmt::format("  v8toolkit_initialize_class_wrappers_{}(isolate);\n", file_count + 1);
            }

            // Close function and file
            class_wrapper_file << "}\n";
            class_wrapper_file.close();

        }








    protected:
	// unique_ptr so references to the WrappedClass objects can be held and not invalidated
        std::vector<unique_ptr<WrappedClass>> wrapped_classes;


        // The value returned here is used internally to run checks against
        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                       llvm::StringRef) {
            return llvm::make_unique<MyASTConsumer>(CI, wrapped_classes);
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
