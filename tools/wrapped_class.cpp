
#include "wrapped_class.h"
#include "parsed_method.h"
#include "class_handler.h"

WrappedClass::WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method) :
    decl(decl),
    class_name(get_canonical_name_for_decl(decl)),
    name_alias(decl->getNameAsString()),
    compiler_instance(compiler_instance),

    my_include(get_include_for_type_decl(compiler_instance, decl)),
    annotations(decl),
    found_method(found_method)
{
    fprintf(stderr, "Creating WrappedClass for record decl ptr: %p\n", (void *)decl);
    string using_name = Annotations::names_for_record_decls[decl];
    if (!using_name.empty()) {
        cerr << fmt::format("Setting name alias for {} to {} because of a 'using' statement", class_name, using_name) << endl;
        name_alias = using_name;
    }


    cerr << "Top of WrappedClass constructor body" << endl;
    if (class_name == "") {
        fprintf(stderr, "%p\n", (void *)decl);
        llvm::report_fatal_error("Empty name string for decl");
    }

    auto pattern = this->decl->getTemplateInstantiationPattern();
    if (pattern && pattern != this->decl) {
        if (!pattern->isDependentType()) {
            llvm::report_fatal_error(fmt::format("template instantiation class's pattern isn't dependent - this is not expected from my understanding: {}", class_name));
        }
    }

    //	    instantiation_pattern = pattern;
    // annotations.merge(Annotations(instantiation_pattern));



    if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) {
        annotations.merge(Annotations(specialization->getSpecializedTemplate()));
    }


    update_wrapped_class_for_type(compiler_instance, *this,
                                  this->decl->getTypeForDecl()->getCanonicalTypeInternal());


    const ClassTemplateSpecializationDecl *specialization = nullptr;
    if ((specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) != nullptr) {
        auto specialized_template = specialization->getSpecializedTemplate();
        auto template_name = specialized_template->getNameAsString();
        template_instantiations[template_name]++;
    }


    for (CXXMethodDecl *method : this->decl->methods()) {
        if (method->hasInheritedPrototype()) {
            cerr << fmt::format("Skipping method {} because it has inherited prototype",
                                method->getNameAsString()) << endl;
            continue;
        }
        this->methods.push_back(make_unique<ParsedMethod>(this->compiler_instance, *this, method));
    }


    cerr << "Final wrapped class annotations: " << endl;
    print_vector(annotations.get());
}

WrappedClass::WrappedClass(const std::string class_name, CompilerInstance & compiler_instance) :
    decl(nullptr),
    class_name(class_name),
    name_alias(class_name),
    compiler_instance(compiler_instance),
    valid(true), // explicitly generated, so must be valid
    found_method(FOUND_GENERATED)
{
    WrappedClass::wrapped_classes.push_back(this);
}



std::string WrappedClass::generate_js_stub() {
    struct MethodParam {
        string type = "";
        string name = "";
        string description = "no description available";

        void convert_type(std::string const & indentation = "") {
            std::smatch matches;
            std::cerr << fmt::format("{} converting {}...", indentation, this->type) << std::endl;
            this->type = regex_replace(type, std::regex("^(struct|class) "), "");
            for (auto &pair : cpp_to_js_type_conversions) {
                if (regex_match(this->type, matches, std::regex(pair.first))) {
                    std::cerr << fmt::format("{} matched {}, converting to {}", indentation, pair.first, pair.second) << std::endl;
                    auto new_type = pair.second; // need a temp because the regex matches point into the current this->type

                    // look for $1, $2, etc in resplacement and substitute in the matching position
                    for (size_t i = 1; i < matches.size(); i++) {
                        // recursively convert the matched type
                        MethodParam mp;
                        mp.type = matches[i].str();
                        mp.convert_type(indentation + "   ");
                        new_type = std::regex_replace(new_type, std::regex(fmt::format("[$]{}", i)),
                                                      mp.type);
                    }
                    this->type = new_type;
                    std::cerr << fmt::format("{}... final conversion to: {}", indentation, this->type) << std::endl;
                }
            }
        }
    }; //  MethodParam

    stringstream result;
    string indentation = "    ";

    result << "/**\n";
    result << fmt::format(" * @class {}\n", this->name_alias);

    for (auto field : this->fields) {
        MethodParam field_type;
        field_type.name = field->getNameAsString();
        field_type.type = field->getType().getAsString();
        field_type.convert_type();
        result << fmt::format(" * @property {{{}}} {} \n", field_type.type, field_type.name);
    }
    result << fmt::format(" **/\n", indentation);


    result << fmt::format("class {}", this->name_alias);

    if (this->base_types.size() == 1) {
        result << fmt::format(" extends {}", (*this->base_types.begin())->name_alias);
    }
    result << "{\n";

    for (auto method : this->methods) {

        auto  method_decl = method->method_decl;
        vector<MethodParam> parameters;
        MethodParam return_value_info;
        string method_description;

        auto parameter_count = method_decl->getNumParams();
        for (unsigned int i = 0; i < parameter_count; i++) {
            MethodParam this_param;
            auto param_decl = method_decl->getParamDecl(i);
            auto parameter_name = param_decl->getNameAsString();
            if (parameter_name == "") {
                data_warning(fmt::format("class {} method {} parameter index {} has no variable name",
                                         this->name_alias, method_decl->getNameAsString(), i));
                parameter_name = fmt::format("unspecified_position_{}", parameters.size());
            }
            this_param.name = parameter_name;
            auto type = get_plain_type(param_decl->getType());
            this_param.type = type.getAsString();
            parameters.push_back(this_param);
        }

        return_value_info.type = get_plain_type(method_decl->getReturnType()).getAsString();

        FullComment *comment = this->compiler_instance.getASTContext().getCommentForDecl(method_decl, nullptr);
        if (comment != nullptr) {
            auto comment_text = get_source_for_source_range(
                this->compiler_instance.getPreprocessor().getSourceManager(), comment->getSourceRange());

            cerr << "FullComment: " << comment_text << endl;
            for (auto i = comment->child_begin(); i != comment->child_end(); i++) {

                auto child_comment_source_range = (*i)->getSourceRange();
                if (child_comment_source_range.isValid()) {

                    auto child_comment_text = get_source_for_source_range(
                        this->compiler_instance.getPreprocessor().getSourceManager(),
                        child_comment_source_range);

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


        result << fmt::format("{}/**\n", indentation);
        for (auto &param : parameters) {
            param.convert_type(); // change to JS types
            result << fmt::format("{} * @param {} {{{}}} {}\n", indentation, param.name, param.type,
                                  param.description);
        }
        return_value_info.convert_type();
        result << fmt::format("{} * @return {{{}}} {}\n", indentation, return_value_info.type,
                              return_value_info.description);
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


    result << fmt::format("}}\n\n\n");
    fprintf(stderr, "%s", result.str().c_str());
    return result.str();
}
