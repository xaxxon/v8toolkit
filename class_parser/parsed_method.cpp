#include <xl/regex/regexer.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/DeclTemplate.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/AST/Comment.h"
#pragma clang diagnostic pop



#include "parsed_method.h"
#include "wrapped_class.h"
#include "clang_helper_functions.h"

#include "ast_action.h"

using namespace xl;

namespace v8toolkit::class_parser {


TypeInfo::TypeInfo(QualType const & type,
                   std::map <std::string, QualType> const & template_parameter_types) :
    template_parameter_types(template_parameter_types),
    type(type)
//name(this->type.getAsString()),
//    name(get_type_string(this->type)),
//    plain_type(get_plain_type(this->type)),
//    plain_name(get_type_string(this->plain_type))
{
//    name = regex_replace(name, std::regex("^(struct|class) "), "");
//    std::cerr << fmt::format("TypeInfo for {} stored {} template parameter types", this->type.getAsString(),
//                             this->template_parameter_types.size()) << std::endl;
//    std::cerr << fmt::format("Created typeinfo for {}", type.getAsString()) << std::endl;

}


TypeInfo::~TypeInfo()
{}




string TypeInfo::get_name() const {
    return substitute_type(this->type, this->template_parameter_types);
}


bool TypeInfo::is_void() const {
    return this->get_name() == "void";
}


TypeInfo TypeInfo::get_plain_type() const {


    QualType plain_type = this->type;
    plain_type = plain_type.getNonReferenceType();
    while (!plain_type->getPointeeType().isNull()) {
        plain_type = plain_type->getPointeeType();
    }
    if (!this->type->isDependentType()) {
        return TypeInfo(plain_type);
    }

    bool was_const_before_substitution = plain_type.isConstQualified();

    // if it's dependent, then look for the substitution
    plain_type = plain_type.getUnqualifiedType();


    plain_type = get_substitution_type_for_type(plain_type, this->template_parameter_types);


    plain_type = plain_type.getNonReferenceType();
    if (was_const_before_substitution) {
        plain_type.addConst();
    }

    while (!plain_type->getPointeeType().isNull()) {
        plain_type = plain_type->getPointeeType();
    }

    return TypeInfo(plain_type);
}


string TypeInfo::get_plain_name() const {
    return get_type_string(this->get_plain_type());
}


string TypeInfo::convert_simple_typename_to_jsdoc(string simple_type_name,
                                                                 std::string const & indentation) {
//    std::cerr << fmt::format("converting '{}' to jsdoc name", simple_type_name) << std::endl;
    // picks off the middle namespace of things like:
    //   std::__cxx11::string as well as std::__1::vector so type names are more predictable
    simple_type_name = regex_replace(simple_type_name, regex("std::__(cxx\\d\\d|\\d)::"), "std::");

    // clang generates type names with class/struct prefixes, remove them
    simple_type_name = regex_replace(simple_type_name, regex("^(class|struct)?\\s*"), "");

    // remove const/volatile
    simple_type_name = regex_replace(simple_type_name, regex("^(?:const\\s*|volatile\\s*)*\\s*(.*?)\\s*&?&?$"), "$1");

    std::smatch matches;

//    std::cerr << fmt::format("trying to convert: {}", simple_type_name) << std::endl;
    for (auto & pair : cpp_to_js_type_conversions) {
        if (regex_match(simple_type_name, matches, std::regex(pair.first))) {
//                std::cerr << fmt::format("{}'{}' matched {}, converting to {}",
//                                         indentation,
//                                         simple_type_name,
//                                         pair.first,
//                                         pair.second) << std::endl;
            return pair.second;
        }
    }


    // no match, return unchanged
//    std::cerr << fmt::format("{}returning simple type name unchanged {}",
//                             indentation, simple_type_name) << std::endl;
    return simple_type_name;
}


string TypeInfo::get_jsdoc_type_name(std::string const & indentation) const {
//    std::cerr << fmt::format("{}converting {}", indentation, this->get_name()) << std::endl;

    string result;

    vector<string> template_type_jsdoc_conversions;
    if (this->is_templated()) {

//        std::cerr << fmt::format("{} is a templated type", this->plain_without_const().get_name()) << std::endl;


        // convert each templated type
        this->for_each_templated_type([&](QualType qualtype) {
            auto typeinfo = TypeInfo(qualtype);
//            std::cerr << fmt::format("{}converting templated type {}", indentation, typeinfo.get_plain_name()) << std::endl;
            template_type_jsdoc_conversions.push_back(typeinfo.get_jsdoc_type_name(indentation + "  "));
        });

        // convert the specialized template name
        string specialized_template_name;
        if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->get_plain_type_decl())) {
            if (auto spec_tmpl = specialization->getSpecializedTemplate()) {
                specialized_template_name = spec_tmpl->getQualifiedNameAsString();
//                fprintf(stderr, "%sSpecialized template: %p, %s\n", indentation.c_str(), (void *) spec_tmpl,
//                        specialized_template_name.c_str());
//                print_vector(Annotations(spec_tmpl).get(), "specialized template annotations", "", false);
            } else {
                llvm::report_fatal_error("couldn't determine name of template being specialized");
            }
        } else {
            llvm::report_fatal_error(
                "Template being specialized couldn't be cast to class template spec decl (shouldn't happen)");
        }

        specialized_template_name = this->convert_simple_typename_to_jsdoc(specialized_template_name, indentation);


        // go through each capturing match and...
        for (size_t i = 0; i < template_type_jsdoc_conversions.size(); i++) {
            // look for $1, $2, etc in replacement and substitute in the matching position
            specialized_template_name = std::regex_replace(specialized_template_name,
                                                           std::regex(fmt::format("\\${}", i + 1)),
                                                           template_type_jsdoc_conversions[i]);
        }
//        std::cerr << fmt::format("{}final jsdoc conversion: {} =? {}",
//                                 indentation, this->get_plain_name(), specialized_template_name)
//                  << std::endl;
        result = specialized_template_name;
    }
        // Handle non-templated types
    else {
//        std::cerr << fmt::format("{} isn't a templated type", this->plain_without_const().get_name()) << std::endl;
        result = this->convert_simple_typename_to_jsdoc(this->get_name(), indentation);
    }


    return result;
}

bool TypeInfo::is_const() const {
    // TODO: should this be calling get_plain_type?  probably not but the behavior may be relied on - need to check
    return this->get_plain_type().type.isConstQualified();
}


TypeInfo TypeInfo::without_const() const {
    QualType qual_type = this->type;
    qual_type.removeLocalConst();
    return TypeInfo(qual_type);
}


CXXRecordDecl const * TypeInfo::get_plain_type_decl() const {
    return this->get_plain_type().type->getAsCXXRecordDecl();
}


// Whether the plain type is templated or not, not whether the original type corresponds to a templated function type parameter
bool TypeInfo::is_templated() const {
    // what about CXXMethodDecl::isFunctionTemplateSpecialization ?
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



void TypeInfo::for_each_templated_type(std::function<void(QualType const &)> callback) const {
    if (auto record_decl = this->get_plain_type().type->getAsCXXRecordDecl()) {
        if (auto specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(record_decl)) {

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
            if (print_logging)
                cerr << "Not a template specializaiton type " << this->get_plain_type().get_name() << endl;
        }
    }
}


DataMember::DataMember(WrappedClass & wrapped_class,
                       WrappedClass & declared_in,
                       FieldDecl * field_decl) :
    wrapped_class(wrapped_class),
    declared_in(declared_in),
    short_name(field_decl->getNameAsString()),
    long_name(field_decl->getQualifiedNameAsString()),
    js_name(short_name),
    type(field_decl->getType()),
    field_decl(field_decl),
    annotations(this->field_decl)
{

    wrapped_class.declaration_count++;

    // the member will be wrapped as const if the actual data type is const or there's an attribute saying it should be const
    this->is_const = this->type.is_const() || annotations.has(V8TOOLKIT_READONLY_STRING);

    // check any typedef annotations as well
    if (auto plain_type_decl = this->type.type->getAsCXXRecordDecl()) {
        this->is_const |= Annotations::annotations_for_record_decls[plain_type_decl].has(V8TOOLKIT_READONLY_STRING);
        std::cerr << fmt::format("done with annotations on typedef, final readonly value: {}", this->is_const) << std::endl;
    }


    FullComment * full_comment = compiler_instance->getASTContext().getCommentForDecl(this->field_decl,
                                                                                                    nullptr);
    if (full_comment != nullptr) {
        auto comment_text = get_source_for_source_range(
            compiler_instance->getPreprocessor().getSourceManager(), full_comment->getSourceRange());
        this->comment = trim_doxygen_comment_whitespace(comment_text);
    }

    this->js_name = this->look_up_js_name();
}


ClassFunction::ParameterInfo::ParameterInfo(ClassFunction & method, int position, ParmVarDecl const * parameter_decl) :
    method(method),
    parameter_decl(parameter_decl),
    position(position),
    type(parameter_decl->getType(), method.template_parameter_types) {
//    std::cerr
//        << fmt::format("In Parameter Info, method has {} template parameter types that were passed in to TypeInfo",
//                       method.template_parameter_types.size()) << std::endl;
    //std::cerr << fmt::format("parameterinfo constructor: parsing parameter {}", name) << std::endl;
    // set the name, give placeholder name if unnamed
    //std::cerr << fmt::format("1") << std::endl;
    this->name = this->parameter_decl->getNameAsString();
    //std::cerr << fmt::format("2") << std::endl;
    if (this->name == "") {
        //std::cerr << fmt::format("3") << std::endl;
        this->name = fmt::format("unspecified_position_{}", this->position);

        // if the parameter has no name, then no comment can be associated with it
        log.warn(LogSubjects::Comments, "class {} method {} parameter index {} has no variable name",
                                 this->method.wrapped_class.class_name, this->method.name, this->position);
    }

    // set default argument or "" if none
    if (parameter_decl->hasDefaultArg()) {
        auto default_argument = parameter_decl->getDefaultArg();
        if (default_argument != nullptr) {
            auto source_range = default_argument->getSourceRange();
            if (source_range.isValid()) {

                auto source = get_source_for_source_range(compiler_instance->getSourceManager(), source_range);

                // certain default values return the = sign, others don't.  specifically  "= {}" comes back with the =, so strip it off
                // is this a clang bug?
                this->default_value = std::regex_replace(source, std::regex("^\\s*=\\s*"), "");

                if (this->default_value == "{}") {

                    this->default_value = fmt::format("{}{{}}", this->type.get_plain_type().without_const().get_name());
                }

            } else {

            }
        } else {
        }
    } else {
        this->default_value = "";
    }
}


ClassFunction::ClassFunction(WrappedClass & wrapped_class,
                             CXXMethodDecl const * method_decl,
                             std::map<string, QualType> const & template_parameter_types,
                             FunctionTemplateDecl const * function_template_decl,
                             std::string const & preferred_js_name) :
    wrapped_class(wrapped_class),
    method_decl(method_decl),
    annotations(this->method_decl),
    is_virtual(method_decl->isVirtual()),
    function_template_decl(function_template_decl),
    template_parameter_types(template_parameter_types),
    return_type(method_decl->getReturnType(), template_parameter_types),
    name(method_decl->getQualifiedNameAsString()),
    js_name(preferred_js_name != "" ? preferred_js_name : method_decl->getNameAsString())
{

//    std::cerr << fmt::format("classfunction: preferred name: '{}' vs default name: '{}'", preferred_js_name, method_decl->getNameAsString()) << std::endl;

//    std::cerr << fmt::format("ClassFunction for {} got {} template substitutions", this->name,
//                             this->template_parameter_types.size()) << std::endl;
    // check to see if there's a name annotation on the method giving it a different JavaScript name
    auto annotated_custom_name = annotations.get_regex(
        "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
    if (!annotated_custom_name.empty()) {
//        std::cerr << fmt::format("Overriding method name {} => {}", this->js_name, annotated_custom_name[0])
//                  << std::endl;
        this->js_name = annotated_custom_name[0];
//        std::cerr << fmt::format("short name is now {}", this->js_name) << std::endl;
    } else {
//        std::cerr << fmt::format("not overriding method name {}", this->js_name) << std::endl;
    }

    this->wrapped_class.declaration_count++;

//    std::cerr << fmt::format("***** Parsing method {}", this->name) << std::endl;


    auto parameter_count = method_decl->getNumParams();
    for (int i = 0; i < parameter_count; i++) {
//        std::cerr << fmt::format("ParsedMethod constructor - parsing parameter {}", i) << std::endl;
        parameters.emplace_back(*this, i, method_decl->getParamDecl(i));

        // make sure the wrapped class has includes for all the types in the method
    }


    // get the comment associated with the method and if there is one, parse it
//    std::cerr << fmt::format("Parsing doxygen comments") << std::endl;
    FullComment * full_comment = compiler_instance->getASTContext().getCommentForDecl(this->method_decl, nullptr);
    if (full_comment != nullptr) {

        auto comment_text = get_source_for_source_range(
            compiler_instance->getPreprocessor().getSourceManager(), full_comment->getSourceRange());

        log.info(LogSubjects::Comments, "full comment: '{}'", comment_text);

        // go through each portion (child) of the full commetn
        int j = 0;
        for (auto i = full_comment->child_begin(); i != full_comment->child_end(); i++) {
            log.info(LogSubjects::Comments, "looking at child comment {} - kind: {} {}", ++j, (*i)->getCommentKindName(),
                                     (*i)->getCommentKind());
            auto child_comment_source_range = (*i)->getSourceRange();
            if (child_comment_source_range.isValid()) {

                auto child_comment_text = get_source_for_source_range(
                    compiler_instance->getPreprocessor().getSourceManager(),
                    child_comment_source_range);

                // if the child comment is a param command comment (describes a parameter)
                if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {

                    // cannot use getParamName() because it crashes if the name doesn't match a parameter
                    auto command_param_name = param_command->getParamNameAsWritten().str();

                    ParameterInfo * matching_parameter_info_ptr = nullptr;
                    for (auto & parameter : this->parameters) {
                        if (command_param_name == parameter.name) {
                            matching_parameter_info_ptr = &parameter;
                            break;
                        }
                    }
//                    auto matching_param_iterator =
//                        std::find_if(parameters.begin(), parameters.end(),
//                                     [&command_param_name](auto &param) {
//                                         return command_param_name == param.name;
//                                     });

//                    std::cerr << fmt::format("found parameter (not matching .end()) {}",
//                                             matching_parameter_info_ptr != nullptr) << std::endl;
//                    std::cerr << fmt::format("has param name?  {}", param_command->hasParamName()) << std::endl;
                    if (param_command->hasParamName() && matching_parameter_info_ptr != nullptr) {

                        auto & param_info = *matching_parameter_info_ptr;
                        if (param_command->getParagraph() != nullptr) {
                            auto parameter_comment = get_source_for_source_range(
                                compiler_instance->getPreprocessor().getSourceManager(),
                                param_command->getParagraph()->getSourceRange());
                            param_info.description = trim_doxygen_comment_whitespace(parameter_comment);
                        }
                    } else {
                        log.warn(LogSubjects::Comments,
                                    "in {}, method parameter comment name '{}' doesn't match any parameter in the function",
                                    this->name,
                                    command_param_name);
                    }
                } else if (auto block_command_comment = dyn_cast<BlockCommandComment>(*i)) {
                    auto block_comment = get_source_for_source_range(
                        compiler_instance->getPreprocessor().getSourceManager(),
                        block_command_comment->getSourceRange());

                    if (auto results = regexer(block_comment, "^[@]return(.*)"_rei)) {
                        this->return_type_comment = trim_doxygen_comment_whitespace(results[1]);
                    }
                } else if (auto paragraph_comment = dyn_cast<ParagraphComment>(*i)) {
                    auto paragraph_comment_text = get_source_for_source_range(
                        compiler_instance->getPreprocessor().getSourceManager(), paragraph_comment->getSourceRange());

                    this->comment = trim_doxygen_comment_whitespace(paragraph_comment_text);

                }
            }
        }
    } else {
//        cerr << "No comment on " << method_decl->getNameAsString() << endl;
    }
}

ClassFunction::~ClassFunction()
{}



//
//std::string ClassFunction::get_js_stub() {
//
//    string indentation = "    ";
//    stringstream result;
//
//    string method_description;
//
//    result << fmt::format("{}/**\n", indentation);
//
////    std::cerr << fmt::format("looking through {} parameters", this->parameters.size()) << std::endl;
//    for (auto &param : this->parameters) {
//      if (param.default_value != "") {
//	result << fmt::format("{} * @param {{{}}} [{} = {}] {}\n", indentation, param.type.get_jsdoc_type_name(),
//			      param.name,
//			      param.default_value,
//			      param.description);
//      } else {
//	result << fmt::format("{} * @param {{{}}} {}\n", indentation, param.type.get_jsdoc_type_name(), param.name,
//			      param.description);
//      }
//    }
//
//    result << fmt::format("{} * @return {{{}}} {}\n", indentation, this->return_type.get_jsdoc_type_name(),
//                          this->return_type_comment);
//    result << fmt::format("{} */\n", indentation);
//
//    result << fmt::format("{}{}{}(", indentation, this->is_static ? "static " : "", this->short_name);
//
//    bool first_parameter = true;
//    for (auto &param : parameters) {
//        if (!first_parameter) {
//            result << ", ";
//        }
//        first_parameter = false;
//        result << fmt::format("{}", param.name);
//    }
//    result << fmt::format("){{}}\n\n");
//
//    return result.str();
//}


string ClassFunction::get_default_argument_tuple_string() const {
    stringstream types;
    stringstream values;
    bool first_default_argument = true;
    for (auto & param : this->parameters) {


        // this should go away once there's proper support
        if (std::regex_search(param.type.get_plain_type().get_name(),
                              std::regex("^(class|struct)?\\s*std::function"))) {
            std::cerr << fmt::format("Cannot handle std::function default parameters yet -- skipping") << std::endl;
            // must clear out all the other defaults since you can't just skip one.
            types = std::stringstream{};
            values = std::stringstream{};
            continue;
        }

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
//        std::cerr << fmt::format("ORIG: {}", param.type.get_name()) << std::endl;
//        std::cerr << fmt::format("POST: {}", remove_local_const_from_type_string(
//            remove_reference_from_type_string(param.type.get_name()))) << std::endl;
        types << remove_local_const_from_type_string(remove_reference_from_type_string(param.type.get_name()));


        // this may have a problem with using type names not visible outside where the default argument is specified
        // may need to substitute the type name instead if it's not a constant and is instead some sort of name
        values << param.default_value;
    }

    return fmt::format("std::tuple<{}>({})", types.str(), values.str());
}

string ClassFunction::get_return_and_class_and_parameter_types_string() const {
    stringstream result;

    result << fmt::format("{}, {}", this->return_type.get_name(), this->wrapped_class.class_name);
    if (!this->parameters.empty()) {
        result << ", ";
        result << this->get_parameter_types_string();
    }

    return result.str();
}

string ClassFunction::get_return_and_parameter_types_string() const {
    stringstream result;

    result << fmt::format("{}", this->return_type.get_name());
    if (!this->parameters.empty()) {
        result << ", ";
        result << this->get_parameter_types_string();
    }

    return result.str();
}

string ClassFunction::get_parameter_types_string() const {
    stringstream result;
    bool first_result = true;
    for (auto & parameter : this->parameters) {
        if (!first_result) {
            result << ", ";
        }
        first_result = false;
        result << fmt::format("{}", parameter.type.get_name());
    }

    return result.str();
}

string ClassFunction::get_js_input_parameter_string() const {
    stringstream result;
    bool first_result = true;
    for (auto & parameter : this->parameters) {
        if (!first_result) {
            result << ", ";
        }
        first_result = false;
        result << fmt::format("{}", parameter.name);
    }

    return result.str();
}


string ClassFunction::get_signature_string() const {
    stringstream result;
    result << this->return_type.get_name() << " ";
    result << this->name << "(";

    bool first = true;
    for (auto & p : this->parameters) {
        if (!first) {
            result << ",";
        }
        first = false;
        result << p.type.get_name();
    }

    result << ")";
//    std::cerr << fmt::format("Returning signature: {}", result.str()) << std::endl;
    return result.str();
}



string MemberFunction::get_signature_string() const {
    auto result = this->ClassFunction::get_signature_string();

    if (this->is_const()) {
        result += " const";
    }
    if (this->is_volatile()) {
        result += " volatile";
    }
    if (this->is_lvalue_qualified()) {
        result += " &";
    } else if (this->is_rvalue_qualified()) {
        result += " &&";
    }
    return result;
}


bool MemberFunction::is_volatile() const {
    return this->method_decl->isVolatile();
}


bool MemberFunction::is_lvalue_qualified() const {
    return this->method_decl->getRefQualifier() == RefQualifierKind::RQ_LValue;
}


bool MemberFunction::is_rvalue_qualified() const {
    return this->method_decl->getRefQualifier() == RefQualifierKind::RQ_RValue;
}


MemberFunction::MemberFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                               map<string, QualType> const & map, FunctionTemplateDecl const * function_template_decl,
                               bool skip_name_check) :
    ClassFunction(wrapped_class, method_decl, map, function_template_decl) {

    for (auto a = method_decl->attr_begin(); a != method_decl->attr_end(); a++) {
//        std::cerr << fmt::format("on function {} looking at attribute {}", this->name, (*a)->getSpelling())
//                  << std::endl;
        if ((*a)->getKind() == attr::Kind::Final) {
//            std::cerr << fmt::format("setting is_virtual_final = true") << std::endl;
            this->is_virtual_final = true;
        }
        if ((*a)->getKind() == attr::Kind::Override) {
//            std::cerr << fmt::format("setting is_virtual_override= true") << std::endl;

            this->is_virtual_override = true;
        }
    };

    if (!skip_name_check) {
        this->js_name = this->look_up_js_name();
    }
}


StaticFunction::StaticFunction(WrappedClass & wrapped_class, CXXMethodDecl const * method_decl,
                               map<string, QualType> const & map, FunctionTemplateDecl const * function_template_decl, bool skip_name_check) :
    ClassFunction(wrapped_class, method_decl, map, function_template_decl) {


    if (!skip_name_check) {
        this->js_name = this->look_up_js_name();
    }
}


ConstructorFunction::ConstructorFunction(WrappedClass & wrapped_class, CXXConstructorDecl const * constructor_decl) :
    ClassFunction(wrapped_class, constructor_decl, {}, nullptr, wrapped_class.get_js_name()),
    constructor_decl(constructor_decl)
{
//    std::cerr << fmt::format("in constructor function constructor body, {} vs {}  -- and {}", wrapped_class.get_name_alias(), wrapped_class.get_short_name(), wrapped_class.decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString()) << std::endl;
//    cerr << "About to get full source for constructor in " << wrapped_class.name_alias << endl;
    auto full_source_loc = FullSourceLoc(constructor_decl->getLocation(),
                                         compiler_instance->getSourceManager());
//    fprintf(stderr, "%s constructor Decl at line %d, file id: %d\n",
//            wrapped_class.name_alias.c_str(),
//            full_source_loc.getExpansionLineNumber(),
//            full_source_loc.getFileID().getHashValue());

    // this should be moved to ClassFunction
//    Annotations constructor_annotations(constructor_decl);
//    auto constructor_name_annotation = constructor_annotations.get_regex(V8TOOLKIT_CONSTRUCTOR_PREFIX "(.*)");
//    // fprintf(stderr,"Got %d annotations on constructor\n", (int)constructor_annotations.size());
//    std::string constructor_name = wrapped_class.name_alias;
//    if (!constructor_name_annotation.empty()) {
//        constructor_name = constructor_name_annotation[0];
//    }

    if (std::find(WrappedClass::used_constructor_names.begin(), WrappedClass::used_constructor_names.end(), this->js_name) !=
        WrappedClass::used_constructor_names.end()) {
        log.error(LogSubjects::Constructors,
                  "Error: duplicate JS constructor function name: {} in class {}",
                        this->js_name.c_str(), this->wrapped_class.get_js_name());
//        for (auto & name : used_constructor_names) {
//            cerr << (fmt::format("Already used constructor name: {}", name)) << endl;
//        }
    } else {
        log.info(LogSubjects::Subjects::Class, "for {}, wrapping constructor {}", wrapped_class.class_name, this->js_name);
        WrappedClass::used_constructor_names.push_back(this->js_name);
    }
}


string ConstructorFunction::generate_js_bindings() {
    stringstream result;
    result << fmt::format("    class_wrapper.add_constructor<{}>(\"{}\", isolate, {});",
                          this->get_parameter_types_string(), this->js_name, this->get_default_argument_tuple_string())
           << endl;

    return result.str();
}


string MemberFunction::generate_js_bindings() {
    stringstream result;
    if (OO_Call == method_decl->getOverloadedOperator()) {
        result << fmt::format("    class_wrapper.make_callable<{}>(&{});\n",
                              this->get_return_and_class_and_parameter_types_string(), this->name);

    } else {
        std::cerr << fmt::format("about to add_method for {}", this->name) << std::endl;
        result << fmt::format("    class_wrapper.add_method<{}>(\"{}\", &{}, {});\n",
                              this->get_return_and_class_and_parameter_types_string(),
                              this->js_name,
                              this->name,
                              this->get_default_argument_tuple_string());
    }
    return result.str();
}


bool MemberFunction::is_callable_overload() const {
    return OO_Call == this->method_decl->getOverloadedOperator();
}


bool MemberFunction::is_const() const {
    return this->method_decl->isConst();
}


string MemberFunction::generate_bidirectional() {
    stringstream result;

    return result.str();

}


string StaticFunction::generate_js_bindings() {
    stringstream result;
    result << fmt::format("    class_wrapper.add_static_method<{}>(\"{}\", &{}, {});\n",
                          this->get_return_and_parameter_types_string(), this->js_name, this->name,
                          this->get_default_argument_tuple_string());
    return result.str();

}


std::string MemberFunction::look_up_js_name() const {

    // first check the config file for overrides
    auto member_function_config =
        PrintFunctionNamesAction::get_config_data()["classes"]
            [this->wrapped_class.class_name]["members"][this->get_signature_string()];

    log.info(LogT::Subjects::ConfigFile, "Looking up member function: {} - {}", this->wrapped_class.class_name,
    this->get_signature_string());
    if (auto name_config_override = member_function_config["name"].get_string()) {
        log.info(LogT::Subjects::ConfigFile, "matched");
        return *name_config_override;
    } else {
        log.info(LogT::Subjects::ConfigFile, "no match");

        // then check code annotations
        auto annotated_custom_name = annotations.get_regex(
            "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
        if (!annotated_custom_name.empty()) {
            return annotated_custom_name[0];
        } else {
            return method_decl->getNameAsString();
        }
    }
}

std::string DataMember::look_up_js_name() const {
    // first check the config file for overrides
    auto member_function_config =
        PrintFunctionNamesAction::get_config_data()["classes"]
        [this->wrapped_class.class_name]["members"][this->long_name];
    log.info(LogT::Subjects::ConfigFile, "Looking up data member: {} - {}", this->wrapped_class.class_name,
             this->long_name);

    if (auto name_config_override = member_function_config["name"].get_string()) {
        log.info(LogT::Subjects::ConfigFile, "matched");

        return *name_config_override;
    } else {

        log.info(LogT::Subjects::ConfigFile, "no match");

        // then check code annotations
        auto annotated_custom_name = annotations.get_regex(
            "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
        if (!annotated_custom_name.empty()) {
            return annotated_custom_name[0];
        } else {
            return this->field_decl->getNameAsString();
        }
    }
}

std::optional<std::string> check_bulk_rename(std::string const & name, std::string const & bulk_rename_type) {
    for(auto rename : PrintFunctionNamesAction::get_config_data()["bulk_renames"][bulk_rename_type].as_array()) {
        auto regex = *rename["regex"].get_string();
        auto replacement = *rename["replace"].get_string();
        xl::Regex r(regex);
        if (r.match(name)) {
            return r.replace(name, replacement);
        }
    }

    // if no substitution found, return empty optional
    return {};
}

std::string StaticFunction::look_up_js_name() const {
    // first check the config file for overrides
    auto member_function_config =
        PrintFunctionNamesAction::get_config_data()["classes"]
        [this->wrapped_class.class_name]["members"][this->get_signature_string()];

    log.info(LogT::Subjects::ConfigFile, "Looking up static function: {} - {}", this->wrapped_class.class_name,
             this->get_signature_string());

    std::string js_name = "";

    if (auto name_config_override = member_function_config["name"].get_string()) {
        log.info(LogT::Subjects::ConfigFile, "match");
        js_name = *name_config_override;
    } else if (auto bulk_rename_result = check_bulk_rename(this->get_signature_string(), "static_functions")) {
        js_name = *bulk_rename_result;
    } else {
        log.info(LogT::Subjects::ConfigFile, "no match");
        // then check code annotations
        auto annotated_custom_name = annotations.get_regex(
            "^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
        if (!annotated_custom_name.empty()) {
            js_name = annotated_custom_name[0];
        } else {
            js_name = method_decl->getNameAsString();
        }
    }

    // TODO: this should move into the config file
    if (static_method_renames.find(this->js_name) != static_method_renames.end()) {
        js_name = static_method_renames[this->js_name];
    }


    // these properties are already used on JavaScript Function objects.
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function
    static std::vector<std::string> javascript_reserved_static_names{
        "name", "arguments", "caller", "length", "displayName", "constructor", "arity"
    };

    if (xl::contains(javascript_reserved_static_names, js_name)) {
        log.error(LogT::Subjects::Methods, "Static function has invalid name: '{}' - static functions cannot be named any of: {}", js_name,
                xl::join(javascript_reserved_static_names));
    }

    return js_name;

}


std::set<std::string> TypeInfo::get_root_includes() const {
    std::set<std::string> includes;


    if (auto function_type = dyn_cast<FunctionType>(&*this->type)) {
        cerr << "IS A FUNCTION TYPE!!!!" << endl;


        // it feels strange, but the type int(bool) from std::function<int(bool)> is a FunctionProtoType
        if (FunctionProtoType const * function_prototype = dyn_cast<FunctionProtoType>(function_type)) {

            std::set<std::string> results;
//            std::cerr << fmt::format("treating as function type: {}", this->get_name()) << std::endl;

//            std::cerr << fmt::format("Getting root includes for return type {}", function_prototype->getReturnType().getAsString()) << std::endl;
            if (auto return_type_include = get_root_include_for_decl(
                TypeInfo(function_prototype->getReturnType()).get_plain_type_decl())) {
                results.insert(*return_type_include);
            }

            for (QualType const & param : function_prototype->param_types()) {

//                std::cerr << fmt::format("Getting root includes for param type {}", param.getAsString()) << std::endl;
                if (auto param_include = get_root_include_for_decl(TypeInfo(param).get_plain_type_decl())) {
                    results.insert(*param_include);
                }
            }
//            std::cerr << fmt::format("returning function root includes") << std::endl;
            return results;
        } else {
//            cerr << "IS NOT A FUNCTION PROTOTYPE" << endl;
        }
    }


//    std::cerr << fmt::format("getting root includes for type ({}) which has {} template parameter types", this->get_name(), this->template_parameter_types.size()) << std::endl;

    if (auto root_include = get_root_include_for_decl(this->get_plain_type_decl())) {
        std::cerr << fmt::format("primary root include for {} is {}", this->get_name(), *root_include) << std::endl;
        includes.insert(*root_include);
    }

    this->for_each_templated_type([&](QualType const & qual_type){
       auto templated_type_includes = TypeInfo(qual_type).get_root_includes();
        includes.insert(templated_type_includes.begin(), templated_type_includes.end());
    });
//    std::cerr << fmt::format("done with all includes for {}", this->get_name()) << std::endl;
    return includes;
}


std::set<std::string> ClassFunction::get_includes() const {
    std::set<std::string> results = this->return_type.get_root_includes();

    for (auto const & param : this->parameters) {
        auto param_includes = param.type.get_root_includes();
        results.insert(param_includes.begin(), param_includes.end());
    }

    auto return_type_includes = this->return_type.get_root_includes();
    results.insert(return_type_includes.begin(), return_type_includes.end());

    return results;
}

std::set<std::string> DataMember::get_includes() const {
    return this->type.get_root_includes();
}


} // end namespace v8toolkit::class_parser