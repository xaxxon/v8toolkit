#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#include "clang.h"

#include "annotations.h"

// Having this too high can lead to VERY memory-intensive compilation units
// Single classes (+base classes) with more than this number of declarations will still be in one file.
// TODO: This should be a command line parameter to the plugin
#define MAX_DECLARATIONS_PER_FILE 40;


extern map<string, string> cpp_to_js_type_conversions;
extern vector<string> base_types_to_ignore;

class WrappedClass;

extern int print_logging;

// if a static method has a name matching the key, change it to the value
extern map<string, string> static_method_renames;
extern map<string, int> template_instantiations;
extern vector<string> base_types_to_ignore;
extern vector<string> types_to_ignore_regex;
extern std::vector<std::string> used_constructor_names;
extern int matched_classes_returned;
extern vector<string> data_warnings;
extern vector<string> data_errors;
extern std::string js_api_header;
extern vector<string> never_include_for_any_file;
extern string header_for_every_class_wrapper_file;
extern vector<string> includes_for_every_class_wrapper_file;
bool has_wrapped_class(const CXXRecordDecl * decl);


// how was a wrapped class determined to be a wrapped class?
enum FOUND_METHOD {
    FOUND_UNSPECIFIED = 0, // no information on why this class is being wrapped - may change later if more information found
    FOUND_ANNOTATION, // this class was annotated as being wrapped
    FOUND_INHERITANCE, // this class is a base of a function that is wrapped
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

template<class T>
std::string join(const T & source, const std::string & between = ", ", bool leading_between = false);
void data_error(const string & error);
void data_warning(const string & warning);
QualType get_plain_type(QualType qual_type);
std::string get_include_for_type_decl(CompilerInstance & compiler_instance, const TypeDecl * type_decl);
void print_vector(const vector<string> & vec, const string & header = "", const string & indentation = "", bool ignore_empty = true);
std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range);
std::string get_source_for_source_range(SourceManager & sm, SourceRange source_range);
std::string get_canonical_name_for_decl(const TypeDecl * decl);
bool is_good_record_decl(const CXXRecordDecl * decl);
std::string get_include_string_for_fileid(CompilerInstance & compiler_instance, FileID & file_id);
std::string get_method_parameters(CompilerInstance & compiler_instance,
                                  WrappedClass & wrapped_class,
                                  const CXXMethodDecl * method,
                                  bool add_leading_comma = false,
                                  bool insert_variable_names = false,
                                  const string & annotation = "");
void update_wrapped_class_for_type(WrappedClass & wrapped_class,
                                   QualType qual_type);

// takes a file number starting at 1 and incrementing 1 each time
// a list of WrappedClasses to print
// and whether or not this is the last file to be written
void write_classes(int file_count, vector<WrappedClass*> & classes, bool last_one);


vector<QualType> get_method_param_qual_types(CompilerInstance & compiler_instance,
                                             const CXXMethodDecl * method,
                                             string const & annotation = "");

vector<string> generate_variable_names(vector<QualType> qual_types, bool with_std_move = false);

void print_specialization_info(const CXXRecordDecl * decl);


std::string get_type_string(QualType qual_type,
                            const std::string & indentation = "");

template<class Callback>
void foreach_constructor(const CXXRecordDecl * klass, Callback && callback,
                         const std::string & annotation = "");

std::string get_method_string(CompilerInstance & compiler_instance,
                              WrappedClass & wrapped_class,
                              const CXXMethodDecl * method);

void generate_javascript_stub(string const &);
void generate_bidirectional_classes(CompilerInstance & compiler_instance);
void generate_bindings();

void data_error(const string & error);
void data_warning(const string & warning);


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
        if (logging == true) {
            return;
        }
        print_logging++;
        logging = true;
    }
};



// joins a range of strings with commas (or whatever specified)
template<class T>
std::string join(const T & source, const std::string & between, bool leading_between) {
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



// calls callback for each constructor in the class.  If annotation specified, only
//   constructors with that annotation will be sent to the callback
template<class Callback>
void foreach_constructor(const CXXRecordDecl * klass, Callback && callback,
                         const std::string & annotation) {

    if (klass == nullptr) {
        cerr << fmt::format("Skipping foreach_constructor because decl was nullptr") << endl;
        return;
    }

    string class_name = klass->getNameAsString();
    if (print_logging) cerr << "Enumerating constructors for " << class_name << " with optional annotation: " << annotation << endl;

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
    cerr << fmt::format("Done enumerating constructors for {}", class_name) << endl;
}


