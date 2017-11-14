#pragma once

#include "helper_functions.h"
#include "clang/AST/Type.h"

namespace v8toolkit::class_parser {


QualType get_plain_type(QualType qual_type);


// for a possibly templated type, return either the stripped down original type or the default template parameter type
QualType get_substitution_type_for_type(QualType original_type, map<string, QualTypeWrapper> const & template_types);


} // end namespace v8toolkit::class_parser