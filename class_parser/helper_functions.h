#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <functional>

#include "clang_fwd.h"
#include "qual_type_wrapper.h"

/*** NO CLANG INCLUDES ALLOWED ***/
// anything requiring a clang include must go in clang_helper_functions.h

#include "annotations.h"
#include "log.h"




namespace v8toolkit::class_parser {


extern vector<pair<string, string>> cpp_to_js_type_conversions;
extern vector<string> base_types_to_ignore;

struct WrappedClass;

inline int print_logging = 0;

// if a static method has a name matching the key, change it to the value
extern map<string, string> static_method_renames;
extern map<string, int> template_instantiations;
extern vector<string> base_types_to_ignore;
extern vector<string> types_to_ignore_regex;
extern int matched_classes_returned;
extern vector<string> never_include_for_any_file;
//extern string header_for_every_class_wrapper_file;
extern vector<string> includes_for_every_class_wrapper_file;

bool has_wrapped_class(const CXXRecordDecl * decl);

std::string get_type_string(QualType const & qual_type,
                            const std::string & indentation = "");


enum EXPORT_TYPE {
    EXPORT_UNSPECIFIED = 0,
    EXPORT_NONE, // export nothing
    EXPORT_ALL
}; // exports everything

// log_subject - what subject to log any messages under, since this is used on different pieces of the AST
EXPORT_TYPE get_export_type(const NamedDecl * decl, LogSubjects::Subjects log_subject, EXPORT_TYPE previous = EXPORT_UNSPECIFIED);

string remove_reference_from_type_string(string const & type_string);

string remove_local_const_from_type_string(string const & type_string);

std::string substitute_type(QualType const & original_type, map<string, QualTypeWrapper> template_types);

void update_wrapped_class_for_type(WrappedClass & wrapped_class,
                                   QualType const & qual_type);


template<class T>
std::string join(const T & source, const std::string & between = ", ", bool leading_between = false);


std::string get_include_for_type_decl(CompilerInstance & compiler_instance, const TypeDecl * type_decl);

void print_vector(const vector<string> & vec, const string & header = "", const string & indentation = "",
                  bool ignore_empty = true);

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


// takes a file number starting at 1 and incrementing 1 each time
// a list of WrappedClasses to print
// and whether or not this is the last file to be written
void write_classes(int file_count, vector<WrappedClass *> & classes, bool last_one);




void print_specialization_info(const CXXRecordDecl * decl);



std::string get_method_string(CompilerInstance & compiler_instance,
                              WrappedClass & wrapped_class,
                              const CXXMethodDecl * method);

void generate_javascript_stub(string const &);

void generate_bidirectional_classes(CompilerInstance & compiler_instance);

void generate_bindings();

class PrintLoggingGuard {
    bool logging = false;
public:
    PrintLoggingGuard() = default;

    ~PrintLoggingGuard() {
        if (logging) {
            print_logging--;
        }
    }

    void log() {
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
        if (!first) { result << between; }
        first = false;
        result << str;
    }
    return result.str();
}

void foreach_constructor(const CXXRecordDecl * klass,
                         std::function<void(CXXConstructorDecl const *)> callback,
                         const std::string & annotation);


std::string trim_doxygen_comment_whitespace(std::string const & comment);



FullComment * get_full_comment_for_decl(CompilerInstance & ci, Decl const * decl, bool any = false);

}