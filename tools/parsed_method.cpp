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




ParsedMethod::ParameterInfo::ParameterInfo(ParsedMethod & method, int position, ParmVarDecl const * parameter_decl, CompilerInstance & compiler_instance) :
    method(method),
    compiler_instance(compiler_instance),
    parameter_decl(parameter_decl),
    position(position),
    type(parameter_decl->getType())
{
    std::cerr << fmt::format("parsing parameter {}", name) << std::endl;
    // set the name, give placeholder name if unnamed
    this->name = this->parameter_decl->getNameAsString();
    if (this->name == "") {
        this->name = fmt::format("unspecified_position_{}", this->position);

        data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                 this->method.wrapped_class.name_alias, this->method.name, this->position));
    }

    // set default argument or "" if none
    if (parameter_decl->hasDefaultArg()) {
        auto default_argument = parameter_decl->getDefaultArg();
        auto source_range = default_argument->getSourceRange();
        auto source = get_source_for_source_range(compiler_instance.getSourceManager(), source_range);
        this->default_value = source;
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
    name(method_decl->getName().str()),
    wrapped_class(wrapped_class),
    is_static(method_decl->isStatic()),
    is_virtual(method_decl->isVirtual())
{
    std::cerr << fmt::format("***** Parsing method {}", this->name) << std::endl;
    auto parameter_count = method_decl->getNumParams();
    for (int i = 0; i < parameter_count; i++) {
        std::cerr << fmt::format("parsing parameter {}", i) << std::endl;
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
        for (auto i = comment->child_begin(); i != comment->child_end(); i++) {

            auto child_comment_source_range = (*i)->getSourceRange();
            if (child_comment_source_range.isValid()) {

                auto child_comment_text = get_source_for_source_range(
                    this->compiler_instance.getPreprocessor().getSourceManager(),
                    child_comment_source_range);

                // if the child comment is a param command comment (describes a parameter)
                if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {
                    cerr << "Is ParamCommandComment" << endl;
                    auto command_param_name = param_command->getParamName(comment).str();

                    auto matching_param_iterator =
                        std::find_if(parameters.begin(), parameters.end(),
                                     [&command_param_name](auto &param) {
                                         return command_param_name == param.name;
                                     });

                    if (param_command->hasParamName() && matching_param_iterator != parameters.end()) {

                        auto &param_info = *matching_param_iterator;
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
                cerr << "Child comment " << (*i)->getCommentKind() << ": " << child_comment_text << endl;
            }
        }
    } else {
        cerr << "No comment on " << method_decl->getNameAsString() << endl;
    }

}




std::string ParsedMethod::get_wrapper_string() {

    string indentation = "    ";
    stringstream result;

    string method_description;
    auto parameter_count = this->parameters.size();

    std::cerr << fmt::format("looking through {} parameters", this->parameters.size()) << std::endl;
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

        result << fmt::format("{}{}(", indentation, method_decl->getNameAsString());
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