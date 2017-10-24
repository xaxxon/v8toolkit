#pragma once

#include <map>


#include "class_parser.h"
#include "clang_fwd.h"
#include "annotations.h"
#include "log.h"

#include "qual_type_wrapper.h"

using namespace std;

namespace v8toolkit::class_parser {


class WrappedClass;


struct LogWatcher {
    vector<LogT::LogMessage> errors;
    void operator()(LogT::LogMessage const & message) {
        if (message.level == LogLevelsT::Levels::Error) {
            this->errors.push_back(message);
        }
    }
};

struct ClassFunction {


    // class the method is in
    WrappedClass & wrapped_class;
    bool is_static;
    bool is_virtual;
    bool is_virtual_final = false;
    bool is_virtual_override = false;
    std::string comment;

    LogWatcher log_watcher;

    // empty if function isn't a template
    FunctionTemplateDecl const * function_template_decl = nullptr;

    // default template type mapping - empty if not templated
    map<string, QualTypeWrapper> template_parameter_types;

    // if this virtual function doesn't exist in a parent class
    bool new_virtual;

    struct TypeInfo {

    private:
        static string convert_simple_typename_to_jsdoc(string simple_type_name, std::string const & = "");

        map<string, QualTypeWrapper> template_parameter_types;

        // the type cannot be gotten because after template substitution there may not be an actual
        //   Type object for the resulting type.  It is only available as a string.  However, the "plain type"
        //   is guaranteed to exist as a Type object
        QualTypeWrapper type;

    public:
        TypeInfo(QualType const & type, map<string, QualTypeWrapper> template_parameter_types = {});
        ~TypeInfo();

        CXXRecordDecl const * get_plain_type_decl() const;

        QualTypeWrapper get_plain_type() const;

        /// name of actual type
        string get_name() const;

        /// name of type without reference or pointers
        string get_plain_name() const;

        /// corresponding javascript type
        string get_jsdoc_type_name(std::string const & = "") const;


        // return if the type (or the type being pointed/referred to) is const (not is the pointer const)
        // double * const => false
        // double const * => true
        bool is_const() const;

        TypeInfo plain_without_const() const;

        bool is_templated() const;

        void for_each_templated_type(std::function<void(QualType const &)>) const;

        bool is_void() const;
    };


    /* PARAMETER INFO */
    class ParameterInfo {
        friend class ClassFunction;

    public:
        ClassFunction & method;
        CompilerInstance & compiler_instance;
        ParmVarDecl const * parameter_decl;
        int position;
        string name;
        string default_value;

        // description of parameter pulled from doxygen comment
        string description = "";

        ParameterInfo(ClassFunction & method, int position, ParmVarDecl const * parameter_decl,
                      CompilerInstance & compiler_instance);


        TypeInfo const type;

    }; // end ParameterInfo

    TypeInfo return_type;
    string return_type_comment;
    vector<ParameterInfo> parameters;
    CXXMethodDecl const * method_decl;

    // c++ name
    string name;

    // name used in javascript
    string js_name;
    CompilerInstance & compiler_instance;
    Annotations annotations;

    string get_default_argument_tuple_string() const;

    // returns string containing function name and input parameters - this function is likely not exactly correct
    //   per c++ standard
    string get_signature_string();

    ClassFunction(WrappedClass & wrapped_class,
                  CXXMethodDecl const * method_decl,
                  std::map<string, QualTypeWrapper> const & template_parameter_types = {},
                  FunctionTemplateDecl const * function_template_decl = nullptr,
                  std::string const & preferred_js_name = "");

    ~ClassFunction();

    // returns true if the methods have the same name and input parameters
    bool compare_signatures(ClassFunction const & other);

    string get_parameter_types_string() const;

    string get_return_and_class_and_parameter_types_string() const;

    string get_return_and_parameter_types_string() const;

    string get_js_input_parameter_string() const;

};


class MemberFunction : public ClassFunction {
public:
    MemberFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                   map<string, QualTypeWrapper> const & map = {},
                   FunctionTemplateDecl const * function_template_decl = nullptr);

    string generate_js_bindings();


    string generate_bidirectional();
};

class StaticFunction : public ClassFunction {
public:
    StaticFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                   map<string, QualTypeWrapper> const & map = {},
                   FunctionTemplateDecl const * function_template_decl = nullptr);

    string generate_js_bindings();

};

class ConstructorFunction : public ClassFunction {

public:
    ConstructorFunction(WrappedClass & wrapped_class, CXXConstructorDecl const * constructor_decl);

    string generate_js_bindings();

    CXXConstructorDecl const * const constructor_decl;

};

struct WrappedClass;

struct DataMember {
    WrappedClass & wrapped_class;
    WrappedClass & declared_in; // level in the hierarchy the member is actually declared at - may match wrapped_class
    string short_name;
    string long_name;
    string js_name;
    ClassFunction::TypeInfo type;
    FieldDecl const * field_decl;
    Annotations annotations;

    // whether the WRAPPING should be const, not necessarily whether the actual c++ type is
    bool is_const = false;

    DataMember(WrappedClass & wrapped_class, WrappedClass & declared_in, FieldDecl * field_decl);

    string get_js_stub();

    string get_bindings();

    /**
     * Doxygen-style comment associated with the data member
     */
    string comment;

};

}