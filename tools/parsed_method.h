#pragma once

#include "class_parser.h"
#include "helper_functions.h"

struct WrappedClass;
struct ParsedMethod {

    // the clang method decl
    CXXMethodDecl const * method_decl;
    string name;

    // class the method is in
    WrappedClass & wrapped_class;
    bool is_static;
    bool is_virtual;
    QualType return_type;
    vector<QualType> parameter_types;

    // default value for every parameter position -- empty string if no default value present
    vector<string> default_values;

    ParsedMethod(CompilerInstance & compiler_instance,
                 WrappedClass & wrapped_class,
                 CXXMethodDecl const * method_decl) :
        method_decl(method_decl),
        name(method_decl->getName().str()),
        wrapped_class(wrapped_class),
        is_static(method_decl->isStatic()),
        is_virtual(method_decl->isVirtual())
    {
        this->return_type = method_decl->getReturnType();

        auto parameter_count = method_decl->getNumParams();
        for (unsigned int i = 0; i < parameter_count; i++) {
            auto param_decl = method_decl->getParamDecl(i);

            if (param_decl->hasDefaultArg()) {
                auto default_argument = param_decl->getDefaultArg();
                auto source_range = default_argument->getSourceRange();
                auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);
                default_values.push_back(source);
            } else {
                default_values.push_back("");
            }

            auto param_qual_type = param_decl->getType();
            parameter_types.push_back(param_qual_type);
        }
    }

    std::string get_wrapper_string();
};
