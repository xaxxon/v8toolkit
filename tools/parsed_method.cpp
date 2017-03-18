#include "parsed_method.h"
#include "wrapped_class.h"
#include <cstdbool>

ParsedMethod::TypeInfo::TypeInfo(QualType const & type) :
    type(type),
    //name(this->type.getAsString()),
    name(get_type_string(this->type)),
    plain_type(get_plain_type(this->type)),
    plain_name(get_type_string(this->plain_type))
{
    name = regex_replace(name, std::regex("^(struct|class) "), "");

    // do any required textual type conversions
    static std::regex bool_conversion = std::regex("^_Bool$");
    name = std::regex_replace(this->name, bool_conversion, "bool");
}


string ParsedMethod::TypeInfo::convert_simple_typename_to_jsdoc(string simple_type_name) {

    // picks off the middle namespace of things like:
    //   std::__cxx11::string as well as std::__1::vector so type names are more predictable
    simple_type_name = regex_replace(simple_type_name, regex("std::__(cxx\\d\\d|\\d)::"), "std::");

    // clang generates type names with class/struct prefixes, remove them
    simple_type_name = regex_replace(simple_type_name, regex("^(class|struct)\\s*"), "");


    std::smatch matches;

    for(auto & conversion_pattern : cpp_to_js_type_conversions) {
        for (auto &pair : cpp_to_js_type_conversions) {
            if (regex_match(simple_type_name, matches, std::regex(pair.first))) {
                std::cerr << fmt::format("matched {}, converting to {}", pair.first, pair.second) << std::endl;
                return pair.second;
            }
        }
    }

    // no match, return unchanged
    std::cerr << fmt::format("returning simple type name unchanged {}", simple_type_name) << std::endl;
    return simple_type_name;
}


string ParsedMethod::TypeInfo::get_jsdoc_type_name() {
    std::cerr << fmt::format("converting {}", this->name) << std::endl;

    vector<string> template_type_jsdoc_conversions;
    if (this->is_templated()) {

        // convert each templated type
        this->for_each_templated_type([&](QualType qualtype){
            auto typeinfo = TypeInfo(qualtype);
            std::cerr << fmt::format("converting templated type {}", typeinfo.plain_name) << std::endl;
            template_type_jsdoc_conversions.push_back(typeinfo.get_jsdoc_type_name());
        });

        // convert the specialized template name
        string specialized_template_name;
        if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->get_plain_type_decl())) {
            if (auto spec_tmpl = specialization->getSpecializedTemplate()) {
                specialized_template_name = spec_tmpl->getQualifiedNameAsString();
                fprintf(stderr, "Specialized template: %p, %s\n", (void *) spec_tmpl,
                        specialized_template_name.c_str());
                print_vector(Annotations(spec_tmpl).get(), "specialized template annotations", "", false);
            } else {
                llvm::report_fatal_error("couldn't determine name of template being specialized");
            }
        } else {
            llvm::report_fatal_error("Template being specialized couldn't be cast to class template spec decl (shouldn't happen)");
        }

        specialized_template_name = this->convert_simple_typename_to_jsdoc(specialized_template_name);


        // go through each capturing match and...
        for (size_t i = 0; i < template_type_jsdoc_conversions.size(); i++) {
            // look for $1, $2, etc in replacement and substitute in the matching position
            specialized_template_name = std::regex_replace(specialized_template_name, std::regex(fmt::format("\\${}", i+1)),
                                          template_type_jsdoc_conversions[i]);
        }
        std::cerr << fmt::format("final jsdoc conversion: {} =? {}", this->plain_name, specialized_template_name) << std::endl;
        return specialized_template_name;
    }
    // Handle non-templated types
    else {
        return this->convert_simple_typename_to_jsdoc(this->plain_name);
    }
}

bool ParsedMethod::TypeInfo::is_const() const {
    return this->plain_type.isConstQualified();
}


ParsedMethod::TypeInfo ParsedMethod::TypeInfo::plain_without_const() const {
    QualType non_const = this->plain_type;
    non_const.removeLocalConst();
    return TypeInfo(non_const);
}



CXXRecordDecl const * ParsedMethod::TypeInfo::get_plain_type_decl() const {
    auto decl = this->plain_type->getAsCXXRecordDecl();
    if (decl == nullptr) {
        return nullptr;
    }
    return dyn_cast<ClassTemplateSpecializationDecl>(decl);
}



bool ParsedMethod::TypeInfo::is_templated() const {
    auto decl = this->get_plain_type_decl();
    if (decl == nullptr) {
        return false;
    }
    if (dyn_cast<ClassTemplateSpecializationDecl>(decl) != nullptr) {
        return true;
    } else {
        return false;
    }
}

void ParsedMethod::TypeInfo::for_each_templated_type(std::function<void(QualType)> callback) const {
    if (auto specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(this->plain_type->getAsCXXRecordDecl())) {


        // go through the template args
        auto & template_arg_list = specialization_decl->getTemplateArgs();
        for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
            auto & arg = template_arg_list[i];

            // this code only cares about types, so skip non-type template arguments
            if (arg.getKind() != clang::TemplateArgument::Type) {
                continue;
            }
            auto template_arg_qual_type = arg.getAsType();
            if (template_arg_qual_type.isNull()) {
                if (print_logging) cerr << "qual type is null" << endl;
                continue;
            }
            callback(template_arg_qual_type);
        }
    } else {
        if (print_logging) cerr << "Not a template specializaiton type " << this->plain_type.getAsString() << endl;
    }
}


DataMember::DataMember(WrappedClass & wrapped_class,
                       WrappedClass & declared_in,
                       FieldDecl * field_decl) :
    wrapped_class(wrapped_class),
    declared_in(declared_in),
    short_name(field_decl->getNameAsString()),
    long_name(field_decl->getQualifiedNameAsString()),
    type(field_decl->getType()),
    field_decl(field_decl),
    annotations(this->field_decl)
{
    wrapped_class.add_name(this->short_name);
    wrapped_class.declaration_count++;

    update_wrapped_class_for_type(wrapped_class, this->type.type);

    // the member will be wrapped as const if the actual data type is const or there's an attribute saying it should be const
    this->is_const = this->type.is_const() || annotations.has(V8TOOLKIT_READONLY_STRING);

}


string DataMember::get_js_stub() {

    stringstream result;

    result << fmt::format(" * @property {{{}}} {} \n", this->type.get_jsdoc_type_name(), this->short_name);

    return result.str();
}

string DataMember::get_bindings() {
    stringstream result;

    if (this->is_const) {
        result << fmt::format("    class_wrapper.add_member_readonly<{}, {}, &{}>(\"{}\");\n",
                              this->type.name,
                              this->declared_in.name_alias, this->long_name, this->short_name);

    } else {
        result << fmt::format("    class_wrapper.add_member<{}, {}, &{}>(\"{}\");\n",
                              this->type.name,
                              this->declared_in.name_alias, this->long_name, this->short_name);
    }

    return result.str();
}


ParsedMethod::ParameterInfo::ParameterInfo(ParsedMethod & method, int position, ParmVarDecl const * parameter_decl, CompilerInstance & compiler_instance) :
    method(method),
    compiler_instance(compiler_instance),
    parameter_decl(parameter_decl),
    position(position),
    type(parameter_decl->getType())
{
    //std::cerr << fmt::format("parameterinfo constructor: parsing parameter {}", name) << std::endl;
    // set the name, give placeholder name if unnamed
    //std::cerr << fmt::format("1") << std::endl;
    this->name = this->parameter_decl->getNameAsString();
    //std::cerr << fmt::format("2") << std::endl;
    if (this->name == "") {
        //std::cerr << fmt::format("3") << std::endl;
        this->name = fmt::format("unspecified_position_{}", this->position);

        data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                 this->method.wrapped_class.name_alias, this->method.short_name, this->position));
    }

    // set default argument or "" if none
    if (parameter_decl->hasDefaultArg()) {
        auto default_argument = parameter_decl->getDefaultArg();
        if (default_argument != nullptr) {
            auto source_range = default_argument->getSourceRange();
            if (source_range.isValid()) {

                auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);

                // certain default values return the = sign, others don't.  specifically  "= {}" comes back with the =, so strip it off
                // is this a clang bug?
                this->default_value = std::regex_replace(source, std::regex("^\\s*=\\s*"), "");

                if (this->default_value == "{}") {

                    this->default_value = fmt::format("{}{{}}", this->type.plain_without_const().name);
                }

            } else {

            }
        } else {
        }
    } else {
        this->default_value = "";
    }

}


ParsedMethod::ParsedMethod(CompilerInstance & compiler_instance,
                           WrappedClass & wrapped_class,
                           CXXMethodDecl const * method_decl) :
    compiler_instance(compiler_instance),
    return_type(method_decl->getReturnType()),
    method_decl(method_decl),
    full_name(method_decl->getQualifiedNameAsString()),
    short_name(method_decl->getNameAsString()),
    wrapped_class(wrapped_class),
    is_static(method_decl->isStatic()),
    is_virtual(method_decl->isVirtual()),
    annotations(this->method_decl)
{

    // check to see if there's a name annotation on the method giving it a different JavaScript name
    auto annotated_custom_name = annotations.get_regex(
        "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
    if (!annotated_custom_name.empty()) {
        std::cerr << fmt::format("Overriding method name {} => {}", this->short_name, annotated_custom_name[0]) << std::endl;
        this->short_name = annotated_custom_name[0];
        std::cerr << fmt::format("short name is now {}", this->short_name) << std::endl;
    } else {
        std::cerr << fmt::format("not overriding method name {}", this->short_name) << std::endl;
    }

    this->wrapped_class.add_name(this->short_name);
    this->wrapped_class.declaration_count++;

    std::cerr << fmt::format("***** Parsing method {}", this->full_name) << std::endl;

    update_wrapped_class_for_type(this->wrapped_class, this->return_type.type);

    auto parameter_count = method_decl->getNumParams();
    for (int i = 0; i < parameter_count; i++) {
        std::cerr << fmt::format("ParsedMethod constructor - parsing parameter {}", i) << std::endl;
        parameters.emplace_back(*this, i, method_decl->getParamDecl(i), this->compiler_instance);

        // make sure the wrapped class has includes for all the types in the method
        update_wrapped_class_for_type(this->wrapped_class, this->parameters.back().type.type);
    }


    // get the comment associated with the method and if there is one, parse it
    std::cerr << fmt::format("Parsing doxygen comments") << std::endl;
    FullComment *comment = this->compiler_instance.getASTContext().getCommentForDecl(this->method_decl, nullptr);
    if (comment != nullptr) {

        auto comment_text = get_source_for_source_range(
            this->compiler_instance.getPreprocessor().getSourceManager(), comment->getSourceRange());

        cerr << "FullComment: " << comment_text << endl;

        // go through each portion (child) of the full commetn
        int j = 0;
        for (auto i = comment->child_begin(); i != comment->child_end(); i++) {
            std::cerr << fmt::format("looking at child comment {}", ++j) << std::endl;
            auto child_comment_source_range = (*i)->getSourceRange();
            if (child_comment_source_range.isValid()) {

                auto child_comment_text = get_source_for_source_range(
                    this->compiler_instance.getPreprocessor().getSourceManager(),
                    child_comment_source_range);

                cerr << "Child comment kind: " << (*i)->getCommentKind() << ": " << child_comment_text << endl;

                // if the child comment is a param command comment (describes a parameter)
                if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {
                    cerr << "Is ParamCommandComment" << endl;
                    if (param_command == nullptr) {
                        std::cerr << fmt::format("THIS CANT BE RIGHT") << std::endl;
                    }
                    std::cerr << fmt::format("param name aswritten: {}", param_command->getParamNameAsWritten().str()) << std::endl;

                    // cannot use getParamName() because it crashes if the name doesn't match a parameter
                    auto command_param_name = param_command->getParamNameAsWritten().str();
                    std::cerr << fmt::format("got command param name {}", command_param_name) << std::endl;

                    ParameterInfo * matching_parameter_info_ptr = nullptr;
                    for(auto & parameter : this->parameters) {
                        std::cerr << fmt::format("comparing {} against {}", command_param_name, parameter.name) << std::endl;
                        if (command_param_name == parameter.name) {
                            std::cerr << fmt::format("found match!") << std::endl;
                            matching_parameter_info_ptr = &parameter;
                            break;
                        }
                    }
//                    auto matching_param_iterator =
//                        std::find_if(parameters.begin(), parameters.end(),
//                                     [&command_param_name](auto &param) {
//                                         return command_param_name == param.name;
//                                     });

                    std::cerr << fmt::format("found parameter (not matching .end()) {}", matching_parameter_info_ptr != nullptr) << std::endl;
                    std::cerr << fmt::format("has param name?  {}", param_command->hasParamName()) << std::endl;
                    if (param_command->hasParamName() && matching_parameter_info_ptr != nullptr) {

                        auto &param_info = *matching_parameter_info_ptr;
                        if (param_command->getParagraph() != nullptr) {
                            param_info.description = get_source_for_source_range(
                                this->compiler_instance.getPreprocessor().getSourceManager(),
                                param_command->getParagraph()->getSourceRange());
                        }
                    } else {
                        data_warning(
                            fmt::format("method parameter comment name doesn't match any parameter {}",
                                        command_param_name));
                    }
                } else {
                    cerr << "is not param command comment" << endl;
                }
            }
        }
    } else {
        cerr << "No comment on " << method_decl->getNameAsString() << endl;
    }

}




std::string ParsedMethod::get_js_stub() {

    string indentation = "    ";
    stringstream result;

    string method_description;

    result << fmt::format("{}/**\n", indentation);

//    std::cerr << fmt::format("looking through {} parameters", this->parameters.size()) << std::endl;
    for (auto &parameter_info : this->parameters) {


        for (auto &param : this->parameters) {
            if (param.default_value != "") {
                result << fmt::format("{} * @param {{{}}} [{} = {}] {}\n", indentation, param.type.get_jsdoc_type_name(),
                                      param.name,
                                      param.default_value,
                                      param.description);
            } else {
                result << fmt::format("{} * @param {{{}}} {}\n", indentation, param.type.get_jsdoc_type_name(), param.name,
                                      param.description);
            }
        }
    }
    result << fmt::format("{} * @return {{{}}} {}\n", indentation, this->return_type.get_jsdoc_type_name(),
                          this->return_type_comment);
    result << fmt::format("{} */\n", indentation);

    result << fmt::format("{}{}{}(", indentation, this->is_static ? "static " : "", this->short_name);

    bool first_parameter = true;
    for (auto &param : parameters) {
        if (!first_parameter) {
            result << ", ";
        }
        first_parameter = false;
        result << fmt::format("{}", param.name);
    }
    result << fmt::format(") {{}}\n\n");

    return result.str();
}


string ParsedMethod::get_default_argument_tuple_string() const {
    stringstream types;
    stringstream values;
    bool first_default_argument = true;
    for (auto & param : this->parameters) {
        if (param.default_value == "") {
            assert(first_default_argument); // if it's not true then there's a gap somehow
            continue;
        }
        if (!first_default_argument) {
            types << ", ";
            values << ", ";
        }
        first_default_argument = false;

        //types << param.type.name; // still has const and references on it, which doesn't work well for tuples
        types << param.type.plain_without_const().name;

        // this may have a problem with using type names not visible outside where the default argument is specified
        // may need to substitute the type name instead if it's not a constant and is instead some sort of name
        values << param.default_value;
    }

    return fmt::format("std::tuple<{}>({})", types.str(), values.str());
}


string ParsedMethod::get_bindings() {
    stringstream result;

    // for add_method
    stringstream return_class_and_parameter_types;

    // for add_static_method
    stringstream return_and_parameter_types;

    return_class_and_parameter_types << fmt::format("{}, {}", this->return_type.name, this->wrapped_class.name_alias);
    return_and_parameter_types << fmt::format("{}", this->return_type.name, this->wrapped_class.name_alias);
    for (auto & parameter : this->parameters) {
        return_class_and_parameter_types << fmt::format(", {}", parameter.type.name);
        return_and_parameter_types << fmt::format(", {}", parameter.type.name);
    }


    // overloaded operator type names (like OO_Call) defined here:
    //   http://llvm.org/reports/coverage/tools/clang/include/clang/Basic/OperatorKinds.def.gcov.html
    // name is "OO_" followed by the first field in each line
    if (OO_Call == method_decl->getOverloadedOperator()) {
        result << fmt::format("    class_wrapper.make_callable<{}>(&{});\n",
                              return_class_and_parameter_types.str(), this->full_name);
    } else if (this->is_static) {

        if (static_method_renames.find(this->short_name) != static_method_renames.end()) {
            this->short_name = static_method_renames[this->short_name];
        }
        this->wrapped_class.has_static_method = true;
        result << fmt::format("    class_wrapper.add_static_method<{}>(\"{}\", &{});\n",
                               return_and_parameter_types.str(), this->short_name, this->full_name);
    } else {
        result << fmt::format("    class_wrapper.add_method<{}>(\"{}\", &{}, {});\n",
                              return_class_and_parameter_types.str(), this->short_name, this->full_name,
        this->get_default_argument_tuple_string());
    }


    return result.str();
}




std::string ParsedMethod::get_bidirectional() {
    llvm::report_fatal_error("Shouldn't be calling this anymore");
//
//    if (!this->method_decl->isVirtual()) {
//        return "";
//    }
//
//    // skip pure virtual functions
//    if (this->method_decl->isPure()) {
//        return "";
//    }
//
//    auto num_params = this->method_decl->getNumParams();
////            printf("Dealing with %s\n", method->getQualifiedNameAsString().c_str());
//    std::stringstream result;
//
//
//    result << "  JS_ACCESS_" << num_params << (this->method_decl->isConst() ? "_CONST(" : "(");
//
//    result << this->return_type.name << ", ";
//
//
//    result << this->short_name;
//
//    if (num_params > 0) {
//        auto types = get_method_param_qual_types(this->compiler_instance, this->method_decl);
//        vector<string>type_names;
//        for (auto & type : types) {
//            type_names.push_back(std::regex_replace(type.getAsString(), std::regex("\\s*,\\s*"), " V8TOOLKIT_COMMA "));
//        }
//
//        result << join(type_names, ", ", true);
//    }
//
//    result  << ");\n";
//
//    return result.str();

}



string ParsedMethod::get_signature_string() {
    stringstream result;
    result << this->short_name << "(";

    bool first = true;
    for (auto & p : this->parameters) {
        if (!first) {
            result << ",";
        }
        first = false;
        result << p.type.name;
    }

    result << ")";
    return result.str();
}
