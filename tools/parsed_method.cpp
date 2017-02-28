#include "parsed_method.h"
#include "wrapped_class.h"


ParsedMethod::TypeInfo::TypeInfo(QualType const & type) :
    type(type),
    name(this->type.getAsString()),
    plain_type(get_plain_type(this->type)),
    plain_name(this->plain_type.getAsString())
{
    this->jsdoc_type_name = convert_type_to_jsdoc(this->plain_name);
}


bool ParsedMethod::TypeInfo::is_const() const {
    return this->plain_type.isConstQualified();
}



DataMember::DataMember(WrappedClass & wrapped_class, FieldDecl * field_decl) :
    wrapped_class(wrapped_class),
    short_name(field_decl->getNameAsString()),
    long_name(field_decl->getQualifiedNameAsString()),
    type(field_decl->getType())
{
    wrapped_class.add_name(this->short_name);
}


string DataMember::get_js_stub() {

    stringstream result;

    result << fmt::format(" * @property {{{}}} {} \n", this->type.jsdoc_type_name, this->short_name);

    return result.str();
}

string DataMember::get_bindings() {
    stringstream result;

    if (this->type.is_const()) {
        result << fmt::format("    class_wrapper.add_member_readonly<{}, {}, &{}>(\"{}\");\n",
                              this->type.name,
                              this->wrapped_class.class_name, this->long_name, this->short_name);

    } else {
        result << fmt::format("    class_wrapper.add_member<{}, {}, &{}>(\"{}\");\n",
                              this->type.name,
                              this->wrapped_class.class_name, this->long_name, this->short_name);
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
    std::cerr << fmt::format("parameterinfo constructor: parsing parameter {}", name) << std::endl;
    // set the name, give placeholder name if unnamed
    std::cerr << fmt::format("1") << std::endl;
    this->name = this->parameter_decl->getNameAsString();
    std::cerr << fmt::format("2") << std::endl;
    if (this->name == "") {
        std::cerr << fmt::format("3") << std::endl;
        this->name = fmt::format("unspecified_position_{}", this->position);

        data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                 this->method.wrapped_class.name_alias, this->method.short_name, this->position));
    }
    std::cerr << fmt::format("4") << std::endl;

    // set default argument or "" if none
    if (parameter_decl->hasDefaultArg()) {
        std::cerr << fmt::format("5") << std::endl;
        auto default_argument = parameter_decl->getDefaultArg();
        if (default_argument != nullptr) {
            std::cerr << fmt::format("5.1") << std::endl;
            auto source_range = default_argument->getSourceRange();
            std::cerr << fmt::format("5.2") << std::endl;
            if (source_range.isValid()) {
                std::cerr << fmt::format("5.3") << std::endl;

                auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);
                std::cerr << fmt::format("5.31") << std::endl;

                this->default_value = source;

            } else {
                std::cerr << fmt::format("5.4") << std::endl;

            }
        } else {
            std::cerr << fmt::format("5.01") << std::endl;
        }
    } else {
        std::cerr << fmt::format("6") << std::endl;
        this->default_value = "";
    }
std::cerr << fmt::format("7") << std::endl;

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

    std::cerr << fmt::format("***** Parsing method {}", this->full_name) << std::endl;
    auto parameter_count = method_decl->getNumParams();
    for (int i = 0; i < parameter_count; i++) {
        std::cerr << fmt::format("ParsedMethod constructor - parsing parameter {}", i) << std::endl;
        parameters.emplace_back(*this, i, method_decl->getParamDecl(i), this->compiler_instance);
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
    auto parameter_count = this->parameters.size();

//    std::cerr << fmt::format("looking through {} parameters", this->parameters.size()) << std::endl;
    for (auto &parameter_info : this->parameters) {


        result << fmt::format("{}/**\n", indentation);
        for (auto &param : this->parameters) {
            if (param.default_value != "") {
                result << fmt::format("{} * @param {{{}}} [{} = {}] {}\n", indentation, param.type.jsdoc_type_name, param.name,
                                      param.default_value,
                                      param.description);
            } else {
                result << fmt::format("{} * @param {{{}}} {}\n", indentation, param.type.jsdoc_type_name, param.name,
                                      param.description);
            }
        }
        result << fmt::format("{} * @return {{{}}} {}\n", indentation, this->return_type.jsdoc_type_name,
                              this->return_type_comment);
        result << fmt::format("{} */\n", indentation);
        if (method_decl->isStatic()) {
            result << fmt::format("{}static ", indentation);
        }

        result << fmt::format("{}{}(", indentation, this->short_name);
        bool first_parameter = true;
        for (auto &param : parameters) {
            if (!first_parameter) {
                result << ", ";
            }
            first_parameter = false;
            result << fmt::format("{}", param.name);
        }
        result << fmt::format("){{}}\n\n");
    }
    return result.str();
}



string ParsedMethod::get_bindings() {
    stringstream result;

    stringstream return_class_and_parameter_types;
    return_class_and_parameter_types << fmt::format("{}, {}", this->return_type.name, this->wrapped_class.name_alias);
    for (auto & parameter : this->parameters) {
        return_class_and_parameter_types << fmt::format(", {}", parameter.type.name);
    }


    if (this->method_decl->isStatic()) {
        if (static_method_renames.find(this->short_name) != static_method_renames.end()) {
            this->short_name = static_method_renames[this->short_name];
        }
    } else {

        // overloaded operator type names (like OO_Call) defined here:
        //   http://llvm.org/reports/coverage/tools/clang/include/clang/Basic/OperatorKinds.def.gcov.html
        // name is "OO_" followed by the first field in each line
        if (OO_Call == method_decl->getOverloadedOperator()) {
                            result << fmt::format("    class_wrapper.make_callable<{}>(&{});\n",
                                                  return_class_and_parameter_types.str(), this->full_name);
        } else {
                            result << fmt::format("    class_wrapper.add_method<{}>(\"{}\", &{});\n",
                                                  return_class_and_parameter_types.str(), this->short_name, this->full_name);
        }
    }

    return result.str();
}
