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
        QualType plain_type;

        string name;
        string plain_name;
        string description;

        string jsdoc_type_name;

        TypeInfo(QualType const & type);

        // returns if the type (or the type being pointed/referred to) is const (not is the pointer const)
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
    string short_name;
    string long_name;
    ParsedMethod::TypeInfo type;
    FieldDecl const * field_decl;

    DataMember(WrappedClass & wrapped_class, FieldDecl * field_decl);

    string get_js_stub();
    string get_bindings();

};