#pragma once

#include "class_parser.h"
#include "helper_functions.h"







struct ParsedMethod {


    // class the method is in
    WrappedClass & wrapped_class;
    bool is_static;
    bool is_virtual;

    // if this virtual function doesn't exist in a parent class
    bool new_virtual;

    struct TypeInfo {
        QualType type;

        // removes reference and all pointers, but keeps qualifications on the "plain type"
        // double const * const **& => double const
        // double * const **& => double
        QualType plain_type;

        /// name of actual type
        string name;

        /// name of type without reference or pointers
        string plain_name;

        /// corresponding javascript type
        string jsdoc_type_name;

        TypeInfo(QualType const & type);

        // return if the type (or the type being pointed/referred to) is const (not is the pointer const)
        // double * const => false
        // double const * => true
        bool is_const() const;
    };


    /* PARAMETER INFO */
    class ParameterInfo {
        friend class ParsedMethod;
    protected:
        ParsedMethod & method;
        CompilerInstance & compiler_instance;
        ParmVarDecl const * parameter_decl;
        int position;
        string name;
        TypeInfo type;
        string default_value;

        // description of parameter pulled from doxygen comment
        string description = "";


    public:
        ParameterInfo(ParsedMethod & method, int position, ParmVarDecl const * parameter_decl, CompilerInstance & compiler_instance);

    }; // end ParameterInfo

    TypeInfo return_type;
    string return_type_comment;
    vector<ParameterInfo> parameters;
    CXXMethodDecl const * method_decl;
    string full_name;
    string short_name;
    CompilerInstance & compiler_instance;
    Annotations annotations;

    string get_default_argument_tuple_string() const;

    // returns string containing function name and input parameters - this function is likely not exactly correct
    //   per c++ standard
    string get_signature_string();

    ParsedMethod(CompilerInstance & compiler_instance,
                 WrappedClass & wrapped_class,
                 CXXMethodDecl const * method_decl);

    string get_js_stub();
    string get_bindings();
    string get_bidirectional();

    // returns true if the methods have the same name and input parameters
    bool compare_signatures(ParsedMethod const & other);
};


struct WrappedClass;
struct DataMember {
    WrappedClass & wrapped_class;
    WrappedClass & declared_in; // level in the hierarchy the member is actually declared at - may match wrapped_class
    string short_name;
    string long_name;
    ParsedMethod::TypeInfo type;
    FieldDecl const * field_decl;
    Annotations annotations;

    // whether the WRAPPING should be const, not necessarily whether the actual c++ type is
    bool is_const = false;

    DataMember(WrappedClass & wrapped_class, WrappedClass & declared_in, FieldDecl * field_decl);

    string get_js_stub();
    string get_bindings();

};