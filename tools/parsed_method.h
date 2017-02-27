#pragma once

#include "class_parser.h"
#include "helper_functions.h"


struct WrappedClass;
struct ParsedMethod {


    // class the method is in
    WrappedClass & wrapped_class;
    bool is_static;
    bool is_virtual;



    struct TypeInfo {
        QualType type;
        QualType plain_type;

        string name;
        string plain_name;
        string description;

        string jsdoc_type_name;

        TypeInfo(QualType const & type);
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
    string name;
    CompilerInstance & compiler_instance;

    ParsedMethod(CompilerInstance & compiler_instance,
                 WrappedClass & wrapped_class,
                 CXXMethodDecl const * method_decl);

    std::string get_wrapper_string();
};
