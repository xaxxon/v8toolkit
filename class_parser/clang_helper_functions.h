#pragma once

#include "helper_functions.h"
#include "clang/AST/Type.h"

namespace v8toolkit::class_parser {


QualType get_plain_type(QualType qual_type);


// for a possibly templated type, return either the stripped down original type or the default template parameter type
QualType get_substitution_type_for_type(QualType original_type, map<string, QualTypeWrapper> const & template_types);




vector<QualType> get_method_param_qual_types(CompilerInstance & compiler_instance,
                                             const CXXMethodDecl * method,
                                             string const & annotation = "");


vector<string> generate_variable_names(vector<QualType> qual_types, bool with_std_move = false);





} // end namespace v8toolkit::class_parser