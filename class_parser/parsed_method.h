#pragma once

#include <map>


#include "class_parser.h"
#include "clang_fwd.h"
#include "annotations.h"
#include "log.h"
#include "type.h"

using namespace std;

namespace v8toolkit::class_parser {


struct WrappedClass;

template<class LogT>
struct LogWatcher {
    vector<typename LogT::LogMessage> errors;
    void operator()(typename LogT::LogMessage const & message) {
        if (message.level == LogT::Levels::Error) {
            this->errors.push_back(message);
        }
    }
};



struct Enum {
    struct Element {
        std::string name;
        int value;
    };

    std::string name;
    std::vector<Element> elements;
};

struct ClassFunction {


    // class the method is in
    WrappedClass & wrapped_class;
    CXXMethodDecl const * method_decl;

    Annotations annotations;


    bool is_static;
    bool is_virtual;
    bool is_virtual_final = false;
    bool is_virtual_override = false;
    std::string comment;

    LogWatcher<LogT> log_watcher;

    // empty if function isn't a template
    FunctionTemplateDecl const * function_template_decl = nullptr;

    // default template type mapping - empty if not templated
    map<string, QualType> template_parameter_types;

    // if this virtual function doesn't exist in a parent class
    bool new_virtual;


    /**
     * Returns the includes for the type of this member function
     * @return all includes needed to use generate code involving this member function
     */
    std::set<std::string> get_includes() const;


    /* PARAMETER INFO */
    class ParameterInfo {
        friend struct ClassFunction;

    public:
        ClassFunction & method;
        ParmVarDecl const * parameter_decl;
        int position;
        string name;
        string default_value;

        // description of parameter pulled from doxygen comment
        string description = "";

        ParameterInfo(ClassFunction & method, int position, ParmVarDecl const * parameter_decl);


        TypeInfo const type;

    }; // end ParameterInfo

    TypeInfo return_type;
    string return_type_comment;
    vector<ParameterInfo> parameters;

    // c++ name
    string name;

    // name used in javascript
    string js_name;

    std::string get_short_name() const;

    string get_default_argument_tuple_string() const;

    // returns string containing function name and input parameters - this function is likely not exactly correct
    //   per c++ standard
    virtual string get_signature_string() const;

    ClassFunction(WrappedClass & wrapped_class,
                  CXXMethodDecl const * method_decl,
                  std::map<string, QualType> const & template_parameter_types = {},
                  FunctionTemplateDecl const * function_template_decl = nullptr,
                  std::string const & preferred_js_name = "");

    ~ClassFunction();

    string get_parameter_types_string() const;

    string get_return_and_class_and_parameter_types_string() const;

    string get_return_and_parameter_types_string() const;

    string get_js_input_parameter_string() const;


};


class MemberFunction : public ClassFunction {

    // finds any name JavaScript name override if present
    std::string look_up_js_name() const;

public:
    MemberFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                   map<string, QualType> const & map = {},
                   FunctionTemplateDecl const * function_template_decl = nullptr, bool skip_name_check = false);

    string generate_js_bindings();

    bool is_callable_overload() const;
    bool is_const() const;
    bool is_lvalue_qualified() const;
    bool is_rvalue_qualified() const;
    bool is_volatile() const;

    string generate_bidirectional();

    string get_signature_string() const override;

};

class StaticFunction : public ClassFunction {

    // finds any name JavaScript name override if present
    std::string look_up_js_name() const;

public:
    StaticFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                   map<string, QualType> const & map = {},
                   FunctionTemplateDecl const * function_template_decl = nullptr,
                   bool skip_name_check = false);

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

private:
    // finds any name JavaScript name override if present
    std::string look_up_js_name() const;


public:
    WrappedClass & wrapped_class;
    WrappedClass & declared_in; // level in the hierarchy the member is actually declared at - may match wrapped_class
    string short_name;
    string long_name;
    string js_name;
    TypeInfo type;
    FieldDecl const * field_decl;
    Annotations annotations;

    // whether the WRAPPING should be const, not necessarily whether the actual c++ type is
    bool is_const = false;

    DataMember(WrappedClass & wrapped_class, WrappedClass & declared_in, FieldDecl * field_decl);

    /**
     * Returns the includes for the type of this data member
     * @return
     */
    std::set<std::string> get_includes() const;

    /**
     * Doxygen-style comment associated with the data member
     */
    string comment;

};

}