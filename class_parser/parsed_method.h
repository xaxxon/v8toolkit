#pragma once

#include "class_parser.h"
#include "helper_functions.h"







struct ClassFunction {


    // class the method is in
    WrappedClass & wrapped_class;
    bool is_static;
    bool is_virtual;

    // if this virtual function doesn't exist in a parent class
    bool new_virtual;

    struct TypeInfo {

    private:
        static string convert_simple_typename_to_jsdoc(string simple_type_name);

    public:
        QualType type;

        // removes reference and all pointers, but keeps qualifications on the "plain type"
        // double const * const **& => double const
        // double * const **& => double
        QualType plain_type;

        CXXRecordDecl const * get_plain_type_decl() const;

        /// name of actual type
        string name;

        /// name of type without reference or pointers
        string plain_name;

        /// corresponding javascript type
        string get_jsdoc_type_name() const;

        TypeInfo(QualType const & type);

        // return if the type (or the type being pointed/referred to) is const (not is the pointer const)
        // double * const => false
        // double const * => true
        bool is_const() const;

        TypeInfo plain_without_const() const;

        bool is_templated() const;
        void for_each_templated_type(std::function<void(QualType)>) const;

        bool is_void() const {return this->type->isVoidType();}
    };


    /* PARAMETER INFO */
    class ParameterInfo {
        friend class ClassFunction;
    protected:
        ClassFunction & method;
        CompilerInstance & compiler_instance;
        ParmVarDecl const * parameter_decl;
        int position;
        string name;
        string default_value;

        // description of parameter pulled from doxygen comment
        string description = "";


    public:
        ParameterInfo(ClassFunction & method, int position, ParmVarDecl const * parameter_decl, CompilerInstance & compiler_instance);
        string generate_js_stub();

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
                 CXXMethodDecl const * method_decl);



    // returns true if the methods have the same name and input parameters
    bool compare_signatures(ClassFunction const & other);

    string get_parameter_types_string() const;
    string get_return_and_class_and_parameter_types_string() const;
    string get_return_and_parameter_types_string() const;
    string get_js_input_parameter_string() const;


};


class MemberFunction : public ClassFunction {
public:
    MemberFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl);
    string generate_js_bindings();
    string generate_js_stub();
    string generate_bidirectional();
};

class StaticFunction : public ClassFunction {
public:
    StaticFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl);
    string generate_js_bindings();
    string generate_js_stub();
};

class ConstructorFunction : public ClassFunction {

public:
    ConstructorFunction(WrappedClass & wrapped_class, CXXConstructorDecl const * constructor_decl);

    string generate_js_bindings();
    string generate_js_stub();

    CXXConstructorDecl const * const constructor_decl;

};

struct WrappedClass;
struct DataMember {
    WrappedClass & wrapped_class;
    WrappedClass & declared_in; // level in the hierarchy the member is actually declared at - may match wrapped_class
    string short_name;
    string long_name;
    ClassFunction::TypeInfo type;
    FieldDecl const * field_decl;
    Annotations annotations;

    // whether the WRAPPING should be const, not necessarily whether the actual c++ type is
    bool is_const = false;

    DataMember(WrappedClass & wrapped_class, WrappedClass & declared_in, FieldDecl * field_decl);

    string get_js_stub();
    string get_bindings();

};