
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/Comment.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/Preprocessor.h"
#pragma clang diagnostic pop


#include <xl/regex/regexer.h>

#include "wrapped_class.h"
#include "parsed_method.h"
#include "class_handler.h"
#include "helper_functions.h"
#include "ast_action.h"

#define FUNC_NO_RTTI
#include "v8toolkit/v8helpers.h"

using namespace xl;

namespace v8toolkit::class_parser {

std::vector<std::string> never_wrap_class_names = {
    "v8toolkit::WrappedClassBase",
    "v8toolkit::EmptyFactoryBase"
};

WrappedClass& WrappedClass::make_wrapped_class(const CXXRecordDecl * decl,
                                               FOUND_METHOD found_method) {

    auto & new_wrapped_class = WrappedClass::wrapped_classes.emplace_back(new WrappedClass(decl, found_method));
    new_wrapped_class->make_bidirectional_wrapped_class_if_needed();
    return *new_wrapped_class;
}


WrappedClass::WrappedClass(const CXXRecordDecl * decl, FOUND_METHOD found_method) :
    class_name(xl::Regex("^(class|struct)?\\s*").replace(get_canonical_name_for_decl(decl), "")),
    decl(decl),
    annotations(decl),
    found_method(found_method)
{

//    std::cerr << fmt::format("WrappedClass constructor for {} at {}", this->class_name, (void*)this) << std::endl;
    log.info(LogSubjects::Subjects::ClassParser, "converting class_name from {} to {}", get_canonical_name_for_decl(decl), this->class_name);

    if (auto matches = xl::Regex("^(class|struct)?\\s*(.*::)?(.*)$").match(this->class_name)) {
        this->class_or_struct = matches[1];
        this->namespace_name = matches[2];
        this->short_name = matches[3];
    } else {
        log.error(LogT::Subjects::Class, "class name doesn't match class name regex");
    }
    
    // This line is very expensive to run on types that will never have a js_name
//    log.info(LogSubjects::Subjects::ClassParser, "js_name for {} set to {}", class_name, get_js_name());

//    std::cerr << fmt::format("created {}", this->class_name) << std::endl;
    if (contains(never_wrap_class_names, this->class_name)) {
//        log.info(LogSubjects::Subjects::ShouldBeWrapped, "{} found in never_wrap_class_names list", this->class_name);
        this->found_method = FOUND_METHOD::FOUND_NEVER_WRAP;
        return;
    } else {
//        std::cerr << fmt::format("{} not found in never wrap class names list", this->class_name) << std::endl;
    }
    log.info(LogSubjects::Class, "Created new WrappedClass: {} {}", this->class_name, (void*)this);
    xl::log::LogCallbackGuard g(log, this->log_watcher);
//    cerr << fmt::format("*** Creating WrappedClass for {} with found_method = {}", this->name_alias, this->found_method) << endl;
//    fprintf(stderr, "Creating WrappedClass for record decl ptr: %p\n", (void *) decl);


//    cerr << "Top of WrappedClass constructor body" << endl;
    if (class_name == "") {
//        fprintf(stderr, "%p\n", (void *) decl);
        llvm::report_fatal_error("Empty name string for decl");
    }

    auto pattern = this->decl->getTemplateInstantiationPattern();
    if (pattern && pattern != this->decl) {
        if (!pattern->isDependentType()) {
            llvm::report_fatal_error(fmt::format(
                "template instantiation class's pattern isn't dependent - this is not expected from my understanding: {}",
                class_name));
        }
    }

    //	    instantiation_pattern = pattern;
    // annotations.merge(Annotations(instantiation_pattern));



    if (auto specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) {
        annotations.merge(Annotations(specialization->getSpecializedTemplate()));
    }




    const ClassTemplateSpecializationDecl * specialization = nullptr;
    if ((specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) != nullptr) {
        auto specialized_template = specialization->getSpecializedTemplate();
        auto template_name = specialized_template->getNameAsString();
        template_instantiations[template_name]++;
    }

//    cerr << "Final wrapped class annotations: " << endl;
//    print_vector(annotations.get());

    bool must_have_base_type = false;
    auto annotation_base_types_to_ignore = this->annotations.get_regex(
        "^" V8TOOLKIT_IGNORE_BASE_TYPE_PREFIX "(.*)$");
    auto annotation_base_type_to_use = this->annotations.get_regex(
        "^" V8TOOLKIT_USE_BASE_TYPE_PREFIX "(.*)$");
    if (annotation_base_type_to_use.size() > 1) {
        log.error(LogSubjects::Class, "More than one base type specified to use for type", this->class_name);
    }

    // if a base type to use is specified, then it must match an actual base type or error
    if (!annotation_base_type_to_use.empty()) {
        must_have_base_type = true;
    }

    this->pimpl_data_member_names = this->annotations.get_regex(
        "^" V8TOOLKIT_USE_PIMPL_PREFIX "(.*)$");
    log.info(LogT::Subjects::Class, "For {} got pimpl data members: {}",
             this->class_name, xl::join(this->pimpl_data_member_names));


//    print_vector(annotation_base_types_to_ignore, "base types to ignore");
//    print_vector(annotation_base_type_to_use, "base type to use");



    // if a comment is directly attached to this class, get it
    FullComment * full_comment = get_full_comment_for_decl(this->decl, false);

    if (full_comment != nullptr) {


        // go through each portion (child) of the full comment
        for (auto i = full_comment->child_begin(); i != full_comment->child_end(); i++) {
//            std::cerr << fmt::format("looking at child comment {}", ++j) << std::endl;
            auto child_comment_source_range = (*i)->getSourceRange();
            if (child_comment_source_range.isValid()) {

                auto child_comment_text = get_source_for_source_range(
                    compiler_instance->getPreprocessor().getSourceManager(),
                    child_comment_source_range);

                log.info(LogSubjects::Comments, "Child comment - kind: {} - '{}'", (*i)->getCommentKind(), child_comment_text);


                // if the child comment is a param command comment (describes a parameter)
                if (auto param_command = dyn_cast<ParamCommandComment>(*i)) {
                    log.info(LogSubjects::Comments, "Is ParamCommandComment");
                    log.info(LogSubjects::Comments,"param name as written: {}", param_command->getParamNameAsWritten().str());

                    // cannot use getParamName() because it crashes if the name doesn't match a parameter
                    auto command_param_name = param_command->getParamNameAsWritten().str();
                    log.info(LogSubjects::Comments, "got command param name {}", command_param_name);

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



    bool found_base_type = false;
    if (print_logging) cerr << "About to process base classes" << endl;
    for (auto base_class : this->decl->bases()) {

        auto base_qual_type = base_class.getType();
        auto base_type_decl = base_qual_type->getAsCXXRecordDecl();
        auto base_type_name = base_type_decl->getNameAsString();
        auto base_type_canonical_name = get_canonical_name_for_decl(base_type_decl);

        if (base_type_canonical_name == "class v8toolkit::WrappedClassBase" &&
            base_class.getAccessSpecifier() != AS_public) {

            log.error(LogSubjects::Class, "class inherits from v8toolkit::WrappedClassBase but not publicly: {}",
                                   this->class_name);
        }

//        cerr << "Base type: " << base_type_canonical_name << endl;
//        for(auto & e : annotation_base_types_to_ignore) {
//            std::cerr << fmt::format("comparing {} and {}", base_type_canonical_name, e) << std::endl;
//        }


        auto base_type_canonical_name_without_class_or_struct = regex_replace(base_type_canonical_name, std::regex("^(struct|class) "), "");

//        std::cerr << fmt::format("annotation base types to ignore list: {}", xl::join(annotation_base_types_to_ignore)) << std::endl;
        if (std::find(annotation_base_types_to_ignore.begin(), annotation_base_types_to_ignore.end(),
                      base_type_canonical_name_without_class_or_struct) !=
            annotation_base_types_to_ignore.end()) {
            v8toolkit::class_parser::log.info(LogT::Subjects::Class, "Skipping base type because it was explicitly excluded in annotation on class: '{}'", base_type_canonical_name_without_class_or_struct);
            continue;
        } else {
//            cerr << "Base type was not explicitly excluded via annotation: " << base_type_canonical_name_without_class_or_struct << endl;
        }
        if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), base_type_canonical_name) !=
            base_types_to_ignore.end()) {
            v8toolkit::class_parser::log.info(LogT::Subjects::Class, "Skipping base type because it was explicitly excluded in plugin base_types_to_ignore: '{}'", base_type_name);
            continue;
        } else {
//            cerr << "Base type was not explicitly excluded via global ignore list" << base_type_name << endl;
        }
        if (!annotation_base_type_to_use.empty() && annotation_base_type_to_use[0] != base_type_name) {
//            cerr << "Skipping base type because it was not the one specified to use via annotation: " << base_type_name << endl;
            continue;
        }

        if (base_qual_type->isDependentType()) {
//            cerr << "-- base type is dependent" << endl;
        }


        found_base_type = true;
        auto base_record_decl = base_qual_type->getAsCXXRecordDecl();

//                fprintf(stderr, "%s -- type class: %d\n", indentation.c_str(), base_qual_type->getTypeClass());
//                cerr << indentation << "-- base type has a cxxrecorddecl" << (record_decl != nullptr) << endl;
//                cerr << indentation << "-- base type has a tagdecl: " << (base_tag_decl != nullptr) << endl;
//                cerr << indentation << "-- can be cast to tagtype: " << (dyn_cast<TagType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to attributed type: " << (dyn_cast<AttributedType>(base_qual_type) != nullptr) << endl;
//                cerr << indentation << "-- can be cast to injected class name type: " << (dyn_cast<InjectedClassNameType>(base_qual_type) != nullptr) << endl;


        if (base_record_decl == nullptr) {
            llvm::report_fatal_error("Got null base type record decl - this should be caught ealier");
        }
//        printf("Found parent/base class %s\n", base_record_decl->getNameAsString().c_str());

//        cerr << "getting base type wrapped class object" << endl;
        WrappedClass & current_base = WrappedClass::get_or_insert_wrapped_class(base_record_decl,
                                                                                this->found_method_means_wrapped()
                                                                                ? FOUND_BASE_CLASS : FOUND_UNSPECIFIED);


        //                printf("For %s, include %s -- for %s, include %s\n", current_base->class_name.c_str(), current_base_include.c_str(), current->class_name.c_str(), current_include.c_str());

        this->add_base_type(current_base);
        current_base.derived_types.insert(this);

        //printf("%s now has %d base classes\n", current->class_name.c_str(), (int)current->base_types.size());
        //printf("%s now has %d derived classes\n", current_base->class_name.c_str(), (int)current_base->derived_types.size());


    } // end processing base classes



    if (auto maybe_root_include = get_root_include_for_decl(this->decl)) {
        this->my_include = *maybe_root_include;
//        std::cerr << fmt::format("Root include for {} is {}", this->class_name, this->my_include) << std::endl;
    } else {
//        std::cerr << fmt::format("no root include for type {}", this->class_name) << std::endl;
    }





    if (print_logging) cerr << "done with base classes" << endl;

    if (must_have_base_type && !found_base_type) {
        log.error(LogSubjects::Class, "base_type_to_use specified but no base type found: {}", this->class_name);
    }

    log.info(LogSubjects::Subjects::Class, "Done creating WrappedClass for {}", this->class_name);
}


void WrappedClass::make_bidirectional_wrapped_class_if_needed() {


// Handle bidirectional class if appropriate
    if (this->annotations.has(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {

// find bidirectional constructor
        vector<QualType> constructor_parameters;

// iterate through all constructors with the specified annotation
        foreach_constructor(this->decl, [&](auto constructor_decl) {
            if (bidirectional_constructor) {
                log.error(LogSubjects::Constructors, "ERROR: Got more than one bidirectional constructor for {}", this->class_name);
                return;
            }
            this->bidirectional_constructor = constructor_decl;
        }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);

        if (this->bidirectional_constructor == nullptr) {
            log.error(LogT::Subjects::Class, "Bidirectional class {} doesn't have a bidirectional constructor explicitly set",
                            this->class_name);
        }

        string bidirectional_class_name = fmt::format("JS{}", this->get_js_name());

// created a WrappedClass for the non-AST JSWrapper class
        WrappedClass & js_wrapped_class = *WrappedClass::wrapped_classes.emplace_back(new WrappedClass(bidirectional_class_name));

        js_wrapped_class.bidirectional = true;
        js_wrapped_class.my_include = fmt::format("\"v8toolkit_generated_bidirectional_{}.h\"", this->get_js_name());



        js_wrapped_class.add_base_type(*this);

        // set the bidirectional class as being a subclass of the non-bidirectional type
        this->derived_types.insert(&js_wrapped_class);

        js_wrapped_class.include_files.insert("<v8toolkit/bidirectional.h>");
    }
}

vector<unique_ptr<ConstructorFunction>> const & WrappedClass::get_constructors() const {
    assert(this->methods_parsed);
    return this->constructors;
}


vector<unique_ptr<MemberFunction>> const & WrappedClass::get_member_functions() const {
    assert(this->methods_parsed);
    return this->member_functions;
}


vector<unique_ptr<StaticFunction>> const & WrappedClass::get_static_functions() const {
    assert(this->methods_parsed);
    return this->static_functions;
}


void WrappedClass::parse_all_methods() {


    if (this->methods_parsed) {
        return;
    }

    xl::log::LogCallbackGuard g(log, this->log_watcher);



    this->methods_parsed = true;

    // do this after setting methods_parsed = true
    //  if it has no decl, then it has no methods, so the methods are effectively already parsed
    if (this->decl == nullptr) {
        return;
    }


    log.info(LogSubjects::Methods, "Parsing class methods for {}", this->class_name);
    
    auto class_config = PrintFunctionNamesAction::get_config_data()["classes"][this->class_name];

    // use decls not methods because methods doesn't give templated functions
    for (Decl * current_decl : this->decl->decls()) {
        
        if (auto named_decl = dyn_cast<NamedDecl>(current_decl)) {
            std::cerr << fmt::format("in {}, looking at named decl {} - {} vs {}\n", 
                this->class_name, named_decl->getNameAsString(),
                                     (void*)this->decl, (void*)named_decl 
                );
            
            // Every class has itself as a nested decl, so skip it
            if (auto nested_record_decl = dyn_cast<CXXRecordDecl>(current_decl)) {
                std::cerr << fmt::format("it's a record decl, too\n");
                if (this->decl->getTypeForDecl() == nested_record_decl->getTypeForDecl()) {
                    std::cerr << fmt::format("skipping nested decl with same type as parent\n");
                    this->my_other_decl = nested_record_decl;
                    continue;
                }
                this->nested_record_decls.push_back(nested_record_decl);
            }
        }
        

        if (auto using_shadow_decl = dyn_cast<UsingShadowDecl>(current_decl)) {
            
//            std::cerr << fmt::format("GOT USING SHADOW DECL") << std::endl;
            auto target_decl = using_shadow_decl->getTargetDecl();
//            std::cerr << fmt::format("target decl name: {}", target_decl->getNameAsString()) << std::endl;
//            std::cerr
//                << fmt::format("target decl is cxxmethoddecl? {}", dyn_cast<CXXMethodDecl>(target_decl) != nullptr)
//                << std::endl;

            if (dyn_cast<CXXMethodDecl>(target_decl) == nullptr) {
                llvm::report_fatal_error(
                    fmt::format("UsingShadowDecl target decl not a CXXMethodDecl (don't know how to handle this): {}",
                                target_decl->getNameAsString()));
            }
//            std::cerr << fmt::format("continuing to parse as if shadow decl target was a method in this class")
//                      << std::endl;
            current_decl = target_decl;
        }



        Annotations decl_annotations(current_decl);
        if (decl_annotations.has(V8TOOLKIT_NONE_STRING)) {
            if (auto named_decl = dyn_cast<NamedDecl>(current_decl)) {
//                std::cerr
//                    << fmt::format("Skipping {} in {} because V8TOOLKIT_NONE_STRING", named_decl->getNameAsString(),
//                                   this->class_name) << std::endl;
            } else {
//                std::cerr
//                    << fmt::format("Skipping non-named_decl in {} because V8TOOLKIT_NONE_STRING", this->class_name)
//                    << std::endl;
            }
        }

        CXXMethodDecl const * method = dyn_cast<CXXMethodDecl>(current_decl);
        std::unordered_map<string, QualType> template_parameter_types;
        FunctionTemplateDecl const * function_template_decl = nullptr;


        if ((function_template_decl = dyn_cast<FunctionTemplateDecl>(current_decl))) {


            method = dyn_cast<CXXMethodDecl>(function_template_decl->getTemplatedDecl());

            if (method == nullptr) {
                llvm::report_fatal_error(fmt::format("FunctionTemplateDecl wasn't a CXXMethodDecl while going through "
                                                         "decl's in {} - not sure what this would mean",
                                                     this->class_name));
            }

            std::string full_method_name(method->getQualifiedNameAsString());

//            std::cerr << fmt::format("templated member function: {}", full_method_name) << std::endl;



            if (Annotations(method).has(V8TOOLKIT_NONE_STRING)) {
    //                std::cerr << fmt::format("SKIPPING TEMPLATE FUNCTION WITH V8TOOLKIT_NONE_STRING") << std::endl;
                continue;
            }


            // store mapping of templated types to default types
            bool all_template_parameters_have_default_value = true;

//            std::cerr << fmt::format("num template parameters for function: {}",
//                                     function_template_decl->getTemplateParameters()->size()) << std::endl;
            auto template_parameters = function_template_decl->getTemplateParameters();
            for (auto i = template_parameters->begin(); i != template_parameters->end(); i++) {
//                std::cerr << fmt::format("template parameter: {}", (*i)->getNameAsString()) << std::endl;

                if (auto template_type_param_decl = dyn_cast<TemplateTypeParmDecl>(*i)) {
//                    std::cerr << fmt::format("--is a type parameter") << std::endl;
                    if (template_type_param_decl->hasDefaultArgument()) {
                        auto default_type = template_type_param_decl->getDefaultArgument();
//                        std::cerr << fmt::format("----has default argument: {}", get_type_string(default_type))
//                                  << std::endl;

//                        std::cerr << fmt::format("In template map: {} => {}", (*i)->getNameAsString(),
//                                                 default_type.getAsString()) << std::endl;
                        template_parameter_types[(*i)->getNameAsString()] = default_type;
                    } else {
                        all_template_parameters_have_default_value = false;
                    }
                } else if (auto template_value_param_decl = dyn_cast<ValueDecl>(*i)) {
//                    std::cerr << fmt::format("--is a value parameter") << std::endl;

                } else {
//                    std::cerr << fmt::format("--is unknown type of parameter") << std::endl;
                }
            }
//            std::cerr << fmt::format("Do all template parameters have defaults? {}",
//                                     all_template_parameters_have_default_value) << std::endl;
            if (!all_template_parameters_have_default_value) {
                continue;

            }
        }

        // if a CXXMethodDecl hasn't been found yet, there's nothing to do for this
        if (!method) {
            continue;
        }





        Annotations method_annotations(method);

        std::string full_method_name = method->getQualifiedNameAsString();
        log.info(LogSubjects::Methods, "looking at {}", full_method_name);

        std::string const signature = [&] {
            if (method->isStatic()) {
                return StaticFunction(*this, method, template_parameter_types, function_template_decl, true).get_signature_string();
            } else {
                return MemberFunction(*this, method, template_parameter_types, function_template_decl, true).get_signature_string();
            }
        }();

        // if the config file has an entry for whether to skip this, use that
        auto member_function_config = class_config["members"][signature];




        if (auto skip = member_function_config["skip"].get_boolean()) {
            v8toolkit::class_parser::log.info(LogT::Subjects::ConfigFile, "Config file says for {}, skip: {}",
                                              signature, *skip);
            if (*skip) {
                continue;
            } else {
                // else it was marked as skip = false, so ignore whether it has an annotation or not
            }
        }
            // else no config entry was found, so check for annotation
        else {

            if (Annotations(method).has(V8TOOLKIT_NONE_STRING)) {
//                std::cerr << fmt::format("SKIPPING TEMPLATE FUNCTION WITH V8TOOLKIT_NONE_STRING") << std::endl;
                continue;
            }
        }


        // this is handled now
//            if (method->isTemplateDecl()) {
//                std::cerr << fmt::format("{} is template decl", full_method_name) << std::endl;
//            }

        if (method->hasInheritedPrototype()) {
            log.info(LogSubjects::Methods, "Skipping method {} because it has inherited prototype", full_method_name);
            continue;
        }

        auto export_type = get_export_type(method, LogSubjects::Methods, EXPORT_ALL);

        if (export_type != EXPORT_ALL) {
            log.info(LogSubjects::Methods, "Skipping method {} because not supposed to be exported %d",
                       full_method_name, export_type);
            continue;
        }

        // only deal with public methods
        if (method->getAccess() != AS_public) {

            auto annotation_list = Annotations(method).get();
            if (!annotation_list.empty()) {
                log.error(LogSubjects::Methods, "Annotation on non-public method: {} - {}", full_method_name, xl::join(annotation_list));
            }
            
            
            log.info(LogSubjects::Methods, "{} is not public, skipping\n", full_method_name);
            continue;
        }

        // list of overloaded operator enumerated values
        // http://llvm.org/reports/coverage/tools/clang/include/clang/Basic/OperatorKinds.def.gcov.html
        if (method->isOverloadedOperator()) {

            // if it's a call operator (operator()), grab it
            if (OO_Call == method->getOverloadedOperator()) {
                // nothing specific to do, just don't skip it only because it's an overloaded operator
            } else {

                // otherwise skip overloaded operators
                log.info(LogSubjects::Methods, "skipping overloaded operator {}", full_method_name.c_str());
                continue;
            }
        }
        if (auto constructor_decl = dyn_cast<CXXConstructorDecl>(method)) {
            // don't deal with constructors on abstract types
            if (this->decl->isAbstract()) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping abstract class constructor");
                continue;
            }
            if (this->annotations.has(V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS_STRING)) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping constructor because DO_NOT_WRAP_CONSTRUCTORS");
                continue;
            }
            if (this->force_no_constructors) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping because force no constructors");
                continue;
            }


            if (constructor_decl->isCopyConstructor()) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping copy constructor");
                continue;
            } else if (constructor_decl->isMoveConstructor()) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping move constructor");
                continue;
            } else if (constructor_decl->isDeleted()) {
                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping deleted constructor");
                continue;
//            } else if (constructor_decl->isImplicit()) {
//                v8toolkit::class_parser::log.info(LogSubjects::Subjects::Constructors, "skipping implicitly created constructor");
//                continue;
            }

            auto new_constructor = std::make_unique<ConstructorFunction>(*this, constructor_decl);
            this->constructors.push_back(std::move(new_constructor));
            continue;
        }

        if (dyn_cast<CXXDestructorDecl>(method)) {
            log.info(LogSubjects::Destructors, "skipping destructor {}", full_method_name);
            continue;
        }

        if (dyn_cast<CXXConversionDecl>(method)) {
            log.info(LogSubjects::Methods, "skipping conversion operator {}", full_method_name);
            continue;
        }


        if (method_annotations.has(V8TOOLKIT_EXTEND_WRAPPER_STRING)) {
            // cerr << "has extend wrapper string" << endl;
            if (!method->isStatic()) {
                log.error(LogSubjects::Methods, "method {} annotated with V8TOOLKIT_EXTEND_WRAPPER must be static",
                                       full_method_name.c_str());

            }
            log.info(LogSubjects::Methods, "skipping static method '{}' marked as v8 class wrapper extension method, but will call it during class wrapping", full_method_name);
            this->wrapper_extension_methods.insert(full_method_name);
            continue; // don't wrap the method as a normal method
        }

        // this is VERY similar to the one above and both probably aren't needed, but they do allow SLIGHTLY different capabilities
        if (method_annotations.has(V8TOOLKIT_CUSTOM_EXTENSION_STRING)) {
            if (!method->isStatic()) {
                log.error(LogSubjects::Methods, "method '{}' annotated with V8TOOLKIT_CUSTOM_EXTENSION must be static",
                                       full_method_name);
                continue;
            } else if (method->getAccess() != AS_public) {
                log.error(LogSubjects::Methods, "method {} annotated with V8TOOLKIT_CUSTOM_EXTENSION must be public", full_method_name);
                continue;
            }
            log.info(LogSubjects::Methods, "skipping static method '{}' marked as V8TOOLKIT_CUSTOM_EXTENSION, but will call it during class wrapping", full_method_name);

            // TODO: don't put the text here, just store the method in WrappedClass
            this->wrapper_custom_extensions.insert(
                fmt::format("class_wrapper.add_new_constructor_function_template_callback(&{});",
                            full_method_name));
            continue; // don't wrap the method as a normal method
        }

//            std::cerr << fmt::format("Creating ParsedMethod...") << std::endl;

        if (method->isStatic()) {
            this->static_functions.push_back(
                make_unique<StaticFunction>(*this, method, template_parameter_types, function_template_decl));
        } else {
            auto member_function = make_unique<MemberFunction>(*this, method, template_parameter_types, function_template_decl);
            if (member_function->is_callable_overload()) {
                this->call_operator_member_function = std::move(member_function);
            } else {
                this->member_functions.push_back(std::move(member_function));
            }
        }

    }
    log.info(LogSubjects::ClassParser, "Done parsing methods on {}", this->class_name);

}



std::vector<Enum> const & WrappedClass::get_enums() const {
    assert(this->enums_parsed);
    return this->enums;
};


void WrappedClass::parse_enums() {
    if (this->enums_parsed) {
        return;
    }

    enums_parsed = true;
    xl::log::LogCallbackGuard g(log, this->log_watcher);


    if (this->decl == nullptr) {
//        std::cerr << fmt::format("No decls for {}", this->name_alias) << std::endl;
        return;
    }

//    std::cerr << fmt::format("about to parse decls for enums in {}", this->name_alias) << std::endl;


    for (auto any_decl : this->decl->decls()) {
        if (auto enum_decl = dyn_cast<EnumDecl>(any_decl)) {
            if (enum_decl == nullptr) {
//                std::cerr << fmt::format("enumdecl is nullptr") << std::endl;
            }
            Enum enum_class;
            enum_class.name = enum_decl->getNameAsString();
//            std::cerr << fmt::format("enum name: {}", enum_decl->getNameAsString()) << std::endl;
            for (EnumConstantDecl * constant_decl : enum_decl->enumerators()) {
                Enum::Element element;
                element.name = constant_decl->getNameAsString();
                element.value = constant_decl->getInitVal().getExtValue();
                enum_class.elements.push_back(std::move(element));
            }
            this->enums.push_back(std::move(enum_class));
        }
    }
};


std::vector<DataMember *> WrappedClass::get_members() const {
    
    assert(this->members_parsed);

    std::vector<DataMember *> results;

    for (auto & member : this->members) {
        results.push_back(member.get());
    }

  

    return results;
}


void WrappedClass::parse_members() {

    if (this->members_parsed) {
        return;
    }
    this->members_parsed = true;

    xl::log::LogCallbackGuard g(log, this->log_watcher);

//    std::cerr << fmt::format("parsing members for {} at {}", this->get_name_alias(), (void*)this) << std::endl;

    this->foreach_inheritance_level([&](WrappedClass & wrapped_class) {
        if (this->decl == nullptr) {
//            std::cerr << fmt::format("No decls for {} while parsing members", this->name_alias) << std::endl;
            return;
        }
        
//        bool is_main_class = &wrapped_class == this;

//        std::cerr << fmt::format("getting fields for {} at {} which has {} base types", wrapped_class.get_name_alias(), (void*)&wrapped_class, wrapped_class.base_types.size()) << std::endl;
//        std::cerr << fmt::format("getting fields for {}", wrapped_class.decl->) << std::endl;

        auto members_config = PrintFunctionNamesAction::get_config_data()["classes"][this->class_name]["members"];
        for (FieldDecl * field : wrapped_class.decl->fields()) {

            std::cerr << fmt::format("looking at field {}\n", field->getNameAsString());
            
            string field_name = field->getQualifiedNameAsString();
            std::string short_field_name = field->getName();

            if (Annotations(field).has(V8TOOLKIT_PIMPL_STRING)) {
                log.info(LogSubjects::Class, "adding pimpl member name because it has V8TOOLKIT_PIMPL");
                this->pimpl_data_member_names.emplace_back(field_name);
            }

            // if this field is a PIMPL field
            if (&wrapped_class == this && xl::contains(this->pimpl_data_member_names, field_name)) {
                auto pimpl_data_member = std::make_unique<DataMember>(*this, wrapped_class, field);

                auto underlying_pimpl_type = WrappedClass::get_wrapped_class(TypeInfo(get_type_from_dereferencing_type(pimpl_data_member->type.type)));
                if (underlying_pimpl_type == nullptr) {
                    log.error(LogSubjects::Class, "pimpl type {} for class {} not found in WrappedClasses", TypeInfo(get_type_from_dereferencing_type(pimpl_data_member->type.type)).get_name(), this->class_name );
                    continue;
                }
                this->pimpl_data_members.push_back(std::move(pimpl_data_member));

                underlying_pimpl_type->found_method = FOUND_METHOD::FOUND_PIMPL; 

                auto pimpl_includes = this->pimpl_data_members.back()->type.get_root_includes();
                std::cerr << fmt::format("adding pimpl includes for {}: {}", this->pimpl_data_members.back()->long_name, xl::join(pimpl_includes)) << std::endl;
                this->include_files.insert(pimpl_includes.begin(), pimpl_includes.end());
                continue;
            }


            // if the config file has an entry for whether to skip this, use that
            auto data_member_config = members_config[field_name];

            if (auto skip = data_member_config["skip"].get_boolean()) {
                v8toolkit::class_parser::log.info(LogT::Subjects::ConfigFile, "Config file says for {}, skip: {}", field_name, *skip);
                if (*skip) {
                    continue;
                } else {
                    // else it was marked as skip = false, so ignore whether it has an annotation or not
                }
            } // else no config entry was found, so check for annotation
            else {
                if (Annotations(field).has(V8TOOLKIT_NONE_STRING)) {
                    continue;
                }
            }

           
            auto export_type = get_export_type(field, LogSubjects::DataMembers, EXPORT_ALL);
            if (export_type == EXPORT_NONE) {
                log.info(LogSubjects::DataMembers, "Skipping data member {} because not supposed to be exported {}",
                           field_name.c_str(), export_type);
                continue;
            }

            if (field->getAccess() != AS_public) {
                auto annotation_list = Annotations(field).get();
                if (!annotation_list.empty() && !(annotation_list.size() == 1 && annotation_list[0] == V8TOOLKIT_PIMPL_STRING)) {
                        log.error(LogSubjects::Methods, "Annotation on non-public member: {}", field_name);
                }

                log.info(LogSubjects::DataMembers, "{} is not public, skipping", field_name);
                continue;
            }

            this->members.emplace_back(make_unique<DataMember>(*this, wrapped_class, field));
        }
    });

    // make sure every pimpl member was found
    if (this->decl != nullptr) { // bidirectional stuff doesn't work right here
        std::cerr << fmt::format("pimpl: expected {} got {}", this->pimpl_data_member_names.size(),
                                 this->pimpl_data_members.size()) << std::endl;
        auto all_pimpl_data_members = this->get_pimpl_data_members();
        if (this->pimpl_data_member_names.size() != all_pimpl_data_members.size()) {
            log.error(LogT::Subjects::Class,
                      "Mismatched number of pimpl members specified vs found in {}: {} specified vs {} found - (specified: {}) (found: {})",
                      this->short_name, this->pimpl_data_member_names.size(), all_pimpl_data_members.size(),
                      xl::join(this->pimpl_data_member_names),
                      xl::join(xl::transform(all_pimpl_data_members, [](auto && e) { return e->long_name; })));
        }
    }

    // none of the pimpl data members can have the same type - that would lead to duplicate names and there's
    //   currently no way to select different names based on PIMPL traversal
    std::set<std::string> types;
    for(auto &pimpl_data_member : this->pimpl_data_members) {
        auto type_string = get_type_string(pimpl_data_member->type.type);
        if (types.count(type_string)) {
            log.error(LogT::Subjects::Class, "multiple pimpl types in {} have the same underlying type: {}", this->class_name, get_type_string(pimpl_data_member->type.type));
        }
        types.insert(type_string);
    }

    for (auto & pimpl_member : this->pimpl_data_members) {

        auto underlying_pimpl_type = get_type_from_dereferencing_type(pimpl_member->type.type);

        auto pimpl_wrapped_class = WrappedClass::get_wrapped_class(underlying_pimpl_type->getAsCXXRecordDecl());
        if (pimpl_wrapped_class == nullptr) {
            log.error(LogT::Subjects::Class, "Pimpl data member's type never seen: {}",
                      underlying_pimpl_type.getAsString());
            continue;
        }

        // this type probably isn't wrapped so need to parse its members explicitly
        pimpl_wrapped_class->parse_members();


        auto pimpl_member_members = pimpl_wrapped_class->get_members();
        if (pimpl_member_members.size() == 0) {
            log.warn(LogT::Subjects::Class, "Pimpl member type has no members");
        }


        std::cerr << fmt::format("setting accessed_through pointers for members in type: {}", pimpl_member->type.get_name()) << std::endl;
        for (auto * pimpl_member_member : pimpl_member_members) {
            if (pimpl_member_member->accessed_through != nullptr &&
                pimpl_member_member->accessed_through != pimpl_member.get()) {
                std::cerr << fmt::format("mismatch {} vs {}\n", pimpl_member_member->accessed_through->long_name, pimpl_member->long_name);
                log.error(LogT::Subjects::Class,
                          "Pimpl member / class already used as pimpl for something else - not allowed: {}",
                          pimpl_member_member->long_name);
            }
            pimpl_member_member->accessed_through = pimpl_member.get();
        }
    }
}


WrappedClass::WrappedClass(const std::string class_name) :
    class_name(class_name),
    decl(nullptr),
    found_method(FOUND_GENERATED)
{
    log.info(LogSubjects::Class, "Created new (no-decl) WrappedClass: '{}'", this->class_name);
}

bool WrappedClass::should_be_parsed() const {
    return this->should_be_wrapped() || this->found_method == FOUND_PIMPL;
}

bool WrappedClass::should_be_wrapped() const {
    

    log.info(LogSubjects::Subjects::ShouldBeWrapped, "In 'should be wrapped' with class {}, annotations: {}", this->class_name, join(annotations.get()));

    if (this->found_method == FOUND_METHOD::FOUND_NEVER_WRAP) {
        log.info(LogSubjects::Subjects::ShouldBeWrapped, "Class marked as FOUND_NEVER_WRAP");
        return false;
    }
    
    if (this->found_method == FOUND_PIMPL) {
        return false;
    }

    // TODO: Isn't this handled in get_exports function?
    if (annotations.has(V8TOOLKIT_NONE_STRING) &&
        annotations.has(V8TOOLKIT_ALL_STRING)) {
        log.error(LogSubjects::Subjects::ShouldBeWrapped, "data error - none and all");
        log.error(LogSubjects::Class, "type has both NONE_STRING and ALL_STRING - this makes no sense", class_name);
    }

    if (found_method == FOUND_BASE_CLASS) {
        log.info(LogSubjects::Subjects::ShouldBeWrapped, "should be wrapped {}- found base class (YES)", this->class_name);
        return true;
    }
    if (found_method == FOUND_GENERATED) {
        log.info(LogSubjects::Subjects::ShouldBeWrapped, "should be wrapped {}- found generated (YES)", this->class_name);
        return true;
    }

    if (found_method == FOUND_INHERITANCE) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            log.info(LogSubjects::Subjects::ShouldBeWrapped, "Found NONE_STRING");
            return false;
        }
    } else if (found_method == FOUND_ANNOTATION) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            log.info(LogSubjects::Subjects::ShouldBeWrapped, "Found NONE_STRING");
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
            llvm::report_fatal_error(
                fmt::format("Type was supposedly found by annotation, but annotation not found: {}", class_name));
        }
    } else if (found_method == FOUND_UNSPECIFIED) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            log.info(LogSubjects::Subjects::ShouldBeWrapped, "Found NONE_STRING on UNSPECIFIED");
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
            log.info(LogSubjects::Subjects::ShouldBeWrapped, "didn't find all string on UNSPECIFIED");
            return false;
        }
        log.info(LogSubjects::Subjects::ShouldBeWrapped, "FOUND_UNSPECIFIED");
        return false;
    }

    /*
      // *** IF A TYPE SHOULD BE WRAPPED THAT FORCES ITS PARENT TYPE TO BE WRAPPED ***


    if (base_types.empty()) {
    cerr << "no base typ so SHOULD BE WRAPPED" << endl;
    return true;
    } else {
    cerr << "Checking should_be_wrapped for base type" << endl;
    auto & base_type_wrapped_class = **base_types.begin();
    cerr << "base type is '" << base_type_wrapped_class.class_name << "'" << endl;
    return base_type_wrapped_class.should_be_wrapped();
    }
    */

    // if it should be wrapped but there are too many base types, error out
    if (base_types.size() > 1) {
        log.error(LogSubjects::Class, "trying to see if {} should be wrapped but it has more than one base type -- unsupported", class_name);
    }


    log.info(LogSubjects::Subjects::ShouldBeWrapped, "should be wrapped -- fall through returning true (YES)");
    return true;
}


bool WrappedClass::ready_for_wrapping(set<WrappedClass const *> dumped_classes) const {



    // don't double wrap yourself
    if (find(dumped_classes.begin(), dumped_classes.end(), this) != dumped_classes.end()) {
//        printf("Already wrapped %s\n", class_name.c_str());
        return false;
    }

    if (!should_be_wrapped()) {
//        cerr << "should be wrapped returned false" << endl;
        return false;
    }

    for (auto base_type : base_types) {
        if (find(dumped_classes.begin(), dumped_classes.end(), base_type) == dumped_classes.end()) {
//            printf("base type %s not already wrapped - return false\n", base_type->class_name.c_str());
            return false;
        }
    }

//    printf("Ready to wrap %s\n", class_name.c_str());

    return true;
}

//
//std::string WrappedClass::get_bindings() {
//    stringstream result;
//    string indentation = "  ";
//
//    result << indentation << "{\n";
//    result << fmt::format("{}  // {}", indentation, class_name) << "\n";
//    result << fmt::format("{}  v8toolkit::V8ClassWrapper<{}> & class_wrapper = isolate.wrap_class<{}>();\n",
//                          indentation, this->class_name, this->class_name);
//    result << fmt::format("{}  class_wrapper.set_class_name(\"{}\");\n", indentation, name_alias);
//
//    for (auto & method : this->get_member_functions()) {
//        result << method->generate_js_bindings();
//    }
//
//    for (auto & method : this->get_static_functions()) {
//        result << method->generate_js_bindings();
//    }
//
//    for (auto & member : members) {
//        result << member->get_bindings();
//    }
//
//    // go through each enum
//    for (auto & enumeration : this->get_enums()) {
//        std::cerr << fmt::format("writing enum to output file: {}", enumeration.first) << std::endl;
//        std::stringstream values;
//        values << "{";
//        auto first_element = true;
//
//        // go through each value in the enum
//        for (auto const & pair : enumeration.second) {
//            if (!first_element) {
//                values << ", ";
//            }
//            first_element = false;
//            values << "{\"" << pair.first << "\", " << pair.second << "}";
//        }
//        values << "}";
//
//        result << fmt::format("{}  class_wrapper.add_enum(\"{}\", {});\n", indentation, enumeration.first, values.str());
//    };
//
//    for (auto & wrapper_extension_method : wrapper_extension_methods) {
//        result << fmt::format("{}  {}\n", indentation, wrapper_extension_method);
//    }
//    for (auto & wrapper_custom_extension : wrapper_custom_extensions) {
//        result << fmt::format("{}  {}\n", indentation, wrapper_custom_extension);
//    }
//
//    if (!derived_types.empty()) {
//        result << fmt::format("{}  class_wrapper.set_compatible_types<{}>();\n", indentation,
//                              get_derived_classes_string());
//    }
//    if (get_base_class_string() != "") {
//        result << fmt::format("{}  class_wrapper.set_parent_type<{}>();\n", indentation,
//                              get_base_class_string());
//    }
//    result << fmt::format("{}  class_wrapper.finalize(true);\n", indentation);
//
//    // if there are no constructors but there are static methods, expose the static methods with this non-constructor name
//    if (this->get_constructors().empty() && this->has_static_method()) {
//        result
//            << fmt::format("{} class_wrapper.expose_static_methods(\"{}\", isolate);\n", indentation, this->name_alias);
//    }
//        // otherwise just create any constructors that may need wrapping (none is fine)
//    else {
//        for (auto & constructor : this->get_constructors()) {
//            result << constructor->generate_js_bindings();
//        }
//    }
//
//    result << indentation << "}\n\n";
//    return result.str();
//}
//
//
//
//
//TODO:
//Get the hierarchy of classes and go to the most derived type and find the include that brings that in.
// - that's the only include that's needed
// - make sure it originates from the TU's .cpp file (file id 1?)
// - possible to work backwards using get_include_string_for_fileid
//   - find where the definition is, then find what included that, what included that, etc until
//     back at fileid 1 then include that first file


bool WrappedClass::is_template_specialization() {
    if (this->decl == nullptr) {
        return false;
    }
    return dyn_cast<ClassTemplateSpecializationDecl>(decl);
}


bool WrappedClass::found_method_means_wrapped() {
    return
        this->found_method == FOUND_ANNOTATION ||
        this->found_method == FOUND_INHERITANCE ||
        this->found_method == FOUND_GENERATED ||
        this->found_method == FOUND_BASE_CLASS;

}

WrappedClass::~WrappedClass() {
//    std::cerr << fmt::format("WrappedClass destructor for {} at {}", this->class_name, (void*)this) << std::endl;
    log.info(LogSubjects::Class, "WrappedClass deleted: {} {}", this->class_name, (void*)this);
}



WrappedClass & WrappedClass::get_or_insert_wrapped_class(const CXXRecordDecl * decl,
                                                         FOUND_METHOD found_method) {

    if (decl->isDependentType()) {
        llvm::report_fatal_error("unpexpected dependent type");
    }

    auto class_name = xl::Regex("^(class|struct)?\\s*").replace(get_canonical_name_for_decl(decl), "");

//    std::cerr << fmt::format("get or insert wrapped class for {} with found method: {}", class_name, found_method) << std::endl;


    // if this decl isn't a definition, get the actual definition
    if (!decl->isThisDeclarationADefinition()) {

        cerr << class_name << " is not a definition - getting definition..." << endl;
        if (!decl->hasDefinition()) {
            llvm::report_fatal_error(fmt::format("{} doesn't have a definition", class_name).c_str());
        }

        decl = decl->getDefinition();
    }


    // go through all the classes which have been seen before
    for (auto & wrapped_class : wrapped_classes) {

        // never wrap NEVER_WRAP classes no matter what
        if (wrapped_class->found_method == FOUND_METHOD::FOUND_NEVER_WRAP) {
            continue;
        }

        // if this one matches another class that's already been seen
        if (wrapped_class->decl && wrapped_class->class_name == class_name) {

            // promote found_method if FOUND_BASE_CLASS is specified - the type must ALWAYS be wrapped
            //   if it is the base of a wrapped type
            if (found_method == FOUND_BASE_CLASS) {
//                std::cerr << fmt::format("{} get_or_insert wrapped class -- matched name and method==found_base_class", class_name) << std::endl;
                // if the class wouldn't otherwise be wrapped, need to make sure no constructors are created
                if (!wrapped_class->should_be_wrapped()) {
                    wrapped_class->force_no_constructors = true;
                }
                wrapped_class->found_method = FOUND_BASE_CLASS;

                // if a type was adjusted, make sure to adjust it's base types as well
                for(auto & base : wrapped_class->base_types) {
//                    std::cerr << fmt::format("running through parent classes of {}", wrapped_class->name_alias) << std::endl;
                    get_or_insert_wrapped_class(base->decl, FOUND_BASE_CLASS);
                }
            }
            //fprintf(stderr, "returning existing object: %p\n", (void *)wrapped_class.get());
            return *wrapped_class;
        }
    }


    return WrappedClass::make_wrapped_class(decl, found_method);
}


std::string WrappedClass::get_short_name() const {
    if (this->decl == nullptr) {
        llvm::report_fatal_error(
            fmt::format("Tried to get_short_name on 'fake' WrappedClass {}", class_name).c_str());
    }
    return decl->getNameAsString();
}


std::string WrappedClass::get_derived_classes_string(int level, const std::string indent) const {
    vector<string> results;
    //            printf("%s In (%d) %s looking at %d derived classes\n", indent.c_str(), level, class_name.c_str(), (int)derived_types.size());
    for (WrappedClass * derived_class : derived_types) {
        results.push_back(derived_class->class_name);
        // only use directly derived types now
        //results.push_back(derived_class->get_derived_classes_string(level + 1, indent + "  "));
    }
    //            printf("%s Returning %s\n", indent.c_str(), join(results).c_str());
    return join(results);
}


void WrappedClass::add_base_type(WrappedClass & base_type) {
    if (xl::contains(base_types_to_ignore, base_type.class_name)) {
        log.info(LogSubjects::Class, "Not adding base type {} to {} because it is in ignore list", base_type.class_name, this->class_name);
        return;
    }

    log.info(LogSubjects::Class, "adding base type {} {} to derived type: {} {}",
             base_type.class_name, (void*)&base_type, this->class_name, (void*)this);

    this->base_types.insert(&base_type);
}


std::string WrappedClass::get_base_class_string() const {

    if (base_types.size() > 1) {
        log.error(LogSubjects::Class, "Type {} has more than one base class - this isn't supported because javascript doesn't support MI\n",
                  class_name);

    }
    return base_types.size() ? (*base_types.begin())->class_name : "";
}


set<ClassFunction const *> WrappedClass::get_all_functions_from_class_hierarchy() const {
    set<ClassFunction const *> results;
    assert(false);
    throw ClassParserException("This isn't implemented");
    return results;
}


bool WrappedClass::has_errors() const {
    return !this->log_watcher.errors.empty();
}


decltype(WrappedClass::log_watcher.errors) const & WrappedClass::get_errors() const {
    return this->log_watcher.errors;
}


void WrappedClass::validate_data() {


    std::cerr << fmt::format("validating {}\n", this->class_name);
    for (auto nested_record_decl : this->nested_record_decls) {
        std::cerr << fmt::format("finding match for nested class {}\n", nested_record_decl->getNameAsString());
        if (auto nested_wrapped_class = WrappedClass::get_wrapped_class(nested_record_decl)) {
            std::cerr << fmt::format("{}'s decl is marked as being nested in {}\n",
                                     nested_wrapped_class->class_name, this->class_name);
            nested_wrapped_class->nested_in = this;
        } else {
            log.error(LogT::Subjects::Class, "Couldn't find wrapped class for: {}", nested_record_decl->getNameAsString());
        }
    }
//
//    for (auto & pimpl_member : this->pimpl_data_members) {
//
//        auto underlying_pimpl_type = get_type_from_dereferencing_type(pimpl_member->type.type);
//
//        auto pimpl_wrapped_class = WrappedClass::get_wrapped_class(underlying_pimpl_type->getAsCXXRecordDecl());
//        if (pimpl_wrapped_class == nullptr) {
//            log.error(LogT::Subjects::Class, "Pimpl data member's type never seen: {}",
//                      underlying_pimpl_type.getAsString());
//            continue;
//        }
//
//        // this type probably isn't wrapped so need to parse its members explicitly
//        pimpl_wrapped_class->parse_members();
//        pimpl_wrapped_class->parse_all_methods();
//    }
    
    
    log.info(LogSubjects::Class, "validating {}", this->class_name);
    xl::log::LogCallbackGuard g(log, this->log_watcher);

    log.info(LogSubjects::Class, "checking for names matching JS reserved words");
    if (xl::contains(reserved_global_names, this->get_js_name())) {
        log.error(LogSubjects::Class, "Class has same name as JavaScript reserved word: {}", this->class_name);
    }

    std::unordered_map<std::string, std::vector<StaticFunction const *>> static_classes_names;
    for(auto & static_function : this->static_functions) {
        static_classes_names[static_function->js_name].push_back(static_function.get());
    }

    log.info(LogSubjects::Class, "checking static function names");
    for (auto & name_pair : static_classes_names) {
        if (name_pair.second.size() > 1) {
            log.error(LogT::Subjects::ClassParser, "Multiple static functions in {} with the same JavaScript name {}: {}",
                      this->class_name, name_pair.first,
                      xl::join(xl::transform(name_pair.second, [](StaticFunction const * static_function){
                          return static_function->get_signature_string();
                      })));
        }
    }
    

    std::unordered_map<std::string, std::vector<std::variant<MemberFunction const *, DataMember const *>>> member_names;
    for(auto & member_function : this->member_functions) {
        member_names[member_function->js_name].push_back(member_function.get());
    }
    for(auto & data_member : this->members) {
        member_names[data_member->js_name].push_back(data_member.get());
    }

    log.info(LogSubjects::Class, "checking member names");
    for (auto & name_pair : member_names) {
        if (name_pair.second.size() > 1) {
            log.error(LogT::Subjects::ClassParser, "Multiple member functions/data members with the same JavaScript name {}: {}", name_pair.first,
                      xl::join(xl::transform(name_pair.second, [](std::variant<MemberFunction const *, DataMember const *> member){
                          if (auto function = std::get_if<MemberFunction const *>(&member)) {
                              return (*function)->get_signature_string();
                          } else if (auto data_member = std::get_if<DataMember const *>(&member)) {
                              return (*data_member)->long_name;
                          } else {
                              assert(false); // unexpected type
                              throw ClassParserException("Unexpected variant type");
                          }
                      })));
        }
    }

    log.info(LogSubjects::Class, "Checking for illegal characters in name");
    if (Regex("(?:[<>:]|^$)").match(this->get_js_name())) {
        log.error(LogSubjects::Subjects::Class,
                  "JavaScript type name '{}' for '{}' is either empty or has one of < > : in it, must be aliased to a standard name", this->get_js_name(), this->class_name);
    }

    if (!this->my_include.empty()) {
        this->include_files.insert(this->my_include);
    }


    // for when the type itself is templated - this picks up the other types that compose this type
    if (this->decl != nullptr) {
        auto my_includes = TypeInfo(this->decl->getTypeForDecl()->getCanonicalTypeInternal()).get_root_includes();
        this->include_files.insert(my_includes.begin(), my_includes.end());
    }

    for(auto base_type : this->base_types) {
//        std::cerr << fmt::format("adding base type include for {} inherits from {}", this->class_name, base_type->class_name) << std::endl;
//        assert(base_type->my_include != ""); // the ordering should preclude this
        if (!base_type->my_include.empty()) {
            this->include_files.insert(base_type->my_include);
        }
    }

    // get all the types for all the data members in the inheritance hierarchy
//    std::cerr << fmt::format("adding data member include files for {}", this->class_name) << std::endl;
    auto data_member_includes = this->foreach_inheritance_level<std::set<std::string>>(
        [&](WrappedClass const & c, std::set<std::string> includes) {
//            std::cerr << fmt::format("getting data members from {}", c.class_name) << std::endl;
            for (auto const & data_member : c.members) {
//                std::cerr << fmt::format("looking at data member {}", data_member->long_name) << std::endl;
                auto data_member_includes = data_member->get_includes();
                includes.insert(data_member_includes.begin(), data_member_includes.end());
            }
            return includes;
        });

    this->include_files.insert(data_member_includes.begin(), data_member_includes.end());

    for(WrappedClass * derived_type : this->derived_types) {
//        std::cerr << fmt::format("adding derived type include for {} inherits from {}", this->class_name, derived_type->class_name) << std::endl;

        if (derived_type->decl != nullptr) {
            auto derived_type_includes = TypeInfo(
                derived_type->decl->getTypeForDecl()->getCanonicalTypeInternal()).get_root_includes();
            this->include_files.insert(derived_type_includes.begin(), derived_type_includes.end());
        }

        // need to have this in the case of bidirectional types that can't compute their actual include requirements
        if (!derived_type->my_include.empty()) {
            this->include_files.insert(derived_type->my_include);
        }
    }

    for (auto const & member_function : this->get_member_functions()) {
//        std::cerr << fmt::format("Looking at member function: {}", member_function->name) << std::endl;
        auto member_function_includes = member_function->get_includes();
        this->include_files.insert(member_function_includes.begin(), member_function_includes.end());
    }

    for (auto const & static_function : this->get_static_functions()) {
//        std::cerr << fmt::format("Looking at static function: {}", static_function->name) << std::endl;
        auto static_function_includes = static_function->get_includes();
        this->include_files.insert(static_function_includes.begin(), static_function_includes.end());
    }

    for (auto const & constructor : this->get_constructors()) {
//        std::cerr << fmt::format("Looking at constructor {}", constructor->name) << std::endl;
        auto constructor_includes = constructor->get_includes();
        this->include_files.insert(constructor_includes.begin(), constructor_includes.end());
    }

    log.info(LogSubjects::Class, "checking for bidirectional constructor if bidirectional");
    if (this->bidirectional) {
        if ((*this->base_types.begin())->bidirectional_constructor == nullptr) {
            log.error(LogSubjects::Class, "Bidirectional class {} has no bidirectional constructor", this->class_name);
        }
    }

    log.info(LogSubjects::Class, "checking for pimpl friend type for {} pimpl members", this->pimpl_data_members.size());
    for (auto & pimpl_type : this->pimpl_data_members) {
        
        // in members of base classes, the friend class will be verified there
        if (&pimpl_type->declared_in != this) {
            std::cerr << fmt::format("{} not declared in {}, skipping", pimpl_type->long_name, this->class_name) << std::endl;
            continue;
        }
//        bool found_wrapper_builder = false;
        std::cerr << fmt::format("{} has friends? {}", this->class_name, this->decl->hasFriends()) << std::endl;
        for (FriendDecl * f : this->decl->friends()) {
            std::cerr << fmt::format("friend decl: {}, friend type: {}", (void *) f, (void *) f->getFriendType())
                      << std::endl;

            // if there's no type, maybe it's a friend function
            if (f->getFriendType() == nullptr) {
                continue;
            }

            auto friend_name = get_type_string(f->getFriendType()->getType().getCanonicalType());
            
            
            // no longer needed with LetMeIn class
//            std::cerr
//                << fmt::format("checking friend name to see if it's the correct wrapper builder: {}", friend_name)
//                << std::endl;
//            if (friend_name == fmt::format("v8toolkit::WrapperBuilder<{}>", pimpl_type->declared_in.class_name)) {
//                found_wrapper_builder = true;
//                break;
//            }
        }
        
        // this is no longer needed with the new LetMeIn type
//        if (!found_wrapper_builder) {
//            log.error(LogT::Subjects::Class, "{} has PIMPL types {} but does not have required friend class v8toolkit::WrapperBuilder<{}>",
//                      this->class_name, pimpl_type->type.get_name(), pimpl_type->declared_in.class_name);
//        } else {
//            log.info(LogT::Subjects::Class, "Found expected WrapperBuilder in type {}", this->short_name);
//        }
    }
    
    
}


std::string const & WrappedClass::get_js_name() const {

//    std::cerr << fmt::format("getting js name for {} with existing js_name = {}", this->class_name, this->js_name) << std::endl;
    if (this->js_name.empty()) {

        // first check the config file for overrides
        auto class_config =
            PrintFunctionNamesAction::get_config_data()["classes"]
            [this->class_name];


        // first try config file
        if (auto name_config_override = class_config["name"].get_string()) {
            log.info(LogT::Subjects::ConfigFile, "Got name JavaScript name from config file: {} => {}", this->class_name, *name_config_override);
            this->js_name = *name_config_override;
        } // then try typedef annotations
        else if (!Annotations::names_for_record_decls[this->decl].empty()) {
            this->js_name = Annotations::names_for_record_decls[this->decl];
        } // then try class annotations
        else {
            // then check code annotations
            auto annotated_custom_name = annotations.get_regex("^" V8TOOLKIT_USE_NAME_PREFIX "(.*)$");
            if (!annotated_custom_name.empty()) {
//                std::cout << fmt::format("found annotated name {}", annotated_custom_name[0] ) << std::endl;
                this->js_name = annotated_custom_name[0];
            } else  {
                auto matches = xl::Regex("^(class|struct)?\\s*(.*::)?(.*)$").match(this->class_name);
                assert(matches);
//                std::cout << fmt::format("got regex name: {} against {}", matches[3], this->class_name) << std::endl;
                this->js_name = matches[3];


            }
        } // last just use the C++ class name

    }

    return this->js_name;
}


WrappedClass * WrappedClass::get_wrapped_class(CXXRecordDecl const * decl) {
    if (decl == nullptr) {
        return nullptr;
    }
    
    for (auto & c : WrappedClass::wrapped_classes) {
        
        if (c->decl == nullptr) {
            // skip 'fake' bidirectional types
            continue;
        }
        
        std::cerr << fmt::format("checking {} for match vs {} ({} (or {}) vs {})\n",
                                 c->class_name, get_canonical_name_for_decl(decl),
                                 (void *) c->decl, (void *) c->my_other_decl, (void *) decl
        );
        if (c->decl == decl) {
            std::cerr << fmt::format("found match!\n");
            return c.get();
        }

        if (c->decl->getTypeForDecl() == decl->getTypeForDecl()) {
            std::cerr << fmt::format("found match (method 2)\n");
            return c.get();
        }
       
    }
    std::cerr << fmt::format("no match vs {} found\n", decl->getNameAsString());
    return nullptr;
}


WrappedClass * WrappedClass::get_wrapped_class(TypeInfo const & type_info) {
    return WrappedClass::get_wrapped_class(type_info.get_plain_type_decl());
}


bool WrappedClass::has_pimpl_members() const {
    return this->pimpl_data_member_names.size() > 0;
}


std::vector<DataMember *> WrappedClass::get_pimpl_data_members(bool with_inherited_members) const {
    
    assert(this->members_parsed);

    std::vector<DataMember *> results;
    
    
    this->foreach_inheritance_level([&](WrappedClass const & wrapped_class) {
        std::cerr << fmt::format("getting pimpl members for {}, {} count: {}\n", wrapped_class.class_name, with_inherited_members, wrapped_class.pimpl_data_members.size());
        if (!with_inherited_members && &wrapped_class != this) {
            return;
        }
        for (auto const & pimpl_member : wrapped_class.pimpl_data_members) {
            results.push_back(pimpl_member.get());
        }
    });
    
//    std::cerr << fmt::format("returning {} results for {}\n", results.size(), this->short_name);
    return results;
}



} // end namespace v8toolkit::class_parser
