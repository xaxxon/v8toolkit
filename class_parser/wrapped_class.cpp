
#include "wrapped_class.h"
#include "parsed_method.h"
#include "class_handler.h"


namespace v8toolkit::class_parser {


// Having this too high can lead to VERY memory-intensive compilation units
// Single classes (+base classes) with more than this number of declarations will still be in one file.
int MAX_DECLARATIONS_PER_FILE = 50;

WrappedClass::WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method) :
    decl(decl),
    class_name(get_canonical_name_for_decl(decl)),
    name_alias(decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString()),
    compiler_instance(compiler_instance),
    my_include(get_include_for_type_decl(compiler_instance, decl)),
    annotations(decl),
    found_method(found_method)
{
//    cerr << fmt::format("*** Creating WrappedClass for {} with found_method = {}", this->name_alias, this->found_method) << endl;
//    fprintf(stderr, "Creating WrappedClass for record decl ptr: %p\n", (void *) decl);
    string using_name = Annotations::names_for_record_decls[decl];
    if (!using_name.empty()) {
        cerr << fmt::format("Setting name alias for {} to {} because of a 'using' statement", class_name, using_name)
             << endl;
        name_alias = using_name;
    }



    // strip off any leading "class " or "struct " off the type name
    name_alias = regex_replace(name_alias, std::regex("^(struct|class) "), "");


    cerr << "Top of WrappedClass constructor body" << endl;
    if (class_name == "") {
        fprintf(stderr, "%p\n", (void *) decl);
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


    update_wrapped_class_for_type(*this,
                                  this->decl->getTypeForDecl()->getCanonicalTypeInternal());


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
        data_error(fmt::format("More than one base type specified to use for type", this->class_name));
    }

    // if a base type to use is specified, then it must match an actual base type or error
    if (!annotation_base_type_to_use.empty()) {
        must_have_base_type = true;
    }


    this->include_files.insert(get_include_for_type_decl(this->compiler_instance, this->decl));


    print_vector(annotation_base_types_to_ignore, "base types to ignore");
    print_vector(annotation_base_type_to_use, "base type to use");


    bool found_base_type = false;
    if (print_logging) cerr << "About to process base classes" << endl;
    for (auto base_class : this->decl->bases()) {

        auto base_qual_type = base_class.getType();
        auto base_type_decl = base_qual_type->getAsCXXRecordDecl();
        auto base_type_name = base_type_decl->getNameAsString();
        auto base_type_canonical_name = get_canonical_name_for_decl(base_type_decl);

        if (base_type_canonical_name == "class v8toolkit::WrappedClassBase" &&
            base_class.getAccessSpecifier() != AS_public) {
            data_error(fmt::format("class inherits from v8toolkit::WrappedClassBase but not publicly: {}",
                                   this->class_name).c_str());
        }

        cerr << "Base type: " << base_type_canonical_name << endl;
        if (std::find(annotation_base_types_to_ignore.begin(), annotation_base_types_to_ignore.end(),
                      base_type_canonical_name) !=
            annotation_base_types_to_ignore.end()) {
            cerr << "Skipping base type because it was explicitly excluded in annotation on class: "
                 << base_type_name << endl;
            continue;
        } else {
            cerr << "Base type was not explicitly excluded via annotation" << endl;
        }
        if (std::find(base_types_to_ignore.begin(), base_types_to_ignore.end(), base_type_canonical_name) !=
            base_types_to_ignore.end()) {
            cerr << "Skipping base type because it was explicitly excluded in plugin base_types_to_ignore: "
                 << base_type_name << endl;
            continue;
        } else {
            cerr << "Base type was not explicitly excluded via global ignore list" << endl;
        }
        if (!annotation_base_type_to_use.empty() && annotation_base_type_to_use[0] != base_type_name) {
            cerr << "Skipping base type because it was not the one specified to use via annotation: "
                 << base_type_name << endl;
            continue;
        }

        if (base_qual_type->isDependentType()) {
            cerr << "-- base type is dependent" << endl;
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
        //  printf("Found parent/base class %s\n", record_decl->getNameAsString().c_str());

        cerr << "getting base type wrapped class object" << endl;
        WrappedClass & current_base = WrappedClass::get_or_insert_wrapped_class(base_record_decl,
                                                                                this->compiler_instance,
                                                                                this->found_method_means_wrapped()
                                                                                ? FOUND_BASE_CLASS : FOUND_UNSPECIFIED);


        auto current_base_include = get_include_for_type_decl(this->compiler_instance, current_base.decl);
        auto current_include = get_include_for_type_decl(this->compiler_instance, this->decl);
        //                printf("For %s, include %s -- for %s, include %s\n", current_base->class_name.c_str(), current_base_include.c_str(), current->class_name.c_str(), current_include.c_str());

        this->include_files.insert(current_base_include);
        current_base.include_files.insert(current_include);
        this->add_base_type(current_base);
        current_base.derived_types.insert(this);

        //printf("%s now has %d base classes\n", current->class_name.c_str(), (int)current->base_types.size());
        //printf("%s now has %d derived classes\n", current_base->class_name.c_str(), (int)current_base->derived_types.size());


    } // end processing base classes

    if (print_logging) cerr << "done with base classes" << endl;
    if (must_have_base_type && !found_base_type) {
        data_error(
            fmt::format("base_type_to_use specified but no base type found: {}", this->class_name));
    }




    // Handle bidirectional class if appropriate
    if (this->annotations.has(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {

        // find bidirectional constructor
        int constructor_parameter_count;
        vector<QualType> constructor_parameters;

        // iterate through all constructors with the specified annotation
        foreach_constructor(this->decl, [&](auto constructor_decl) {
            if (bidirectional_constructor) {
                data_error(fmt::format("ERROR: Got more than one bidirectional constructor for {}", this->name_alias));
                return;
            }
            this->bidirectional_constructor = constructor_decl;
        }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);

        if (this->bidirectional_constructor == nullptr) {
            this->set_error(
                fmt::format("Bidirectional class {} doesn't have a bidirectional constructor explicitly set",
                            this->name_alias));
        }

        string bidirectional_class_name = fmt::format("JS{}", this->name_alias);

        // created a WrappedClass for the non-AST JSWrapper class
        WrappedClass & js_wrapped_class = WrappedClass::wrapped_classes.emplace_back(bidirectional_class_name,
                                                                                     this->compiler_instance);

        js_wrapped_class.bidirectional = true;
        js_wrapped_class.my_include = fmt::format("\"v8toolkit_generated_bidirectional_{}.h\"", this->name_alias);
        cerr << fmt::format("my_include for bidirectional class: {}", js_wrapped_class.my_include) << endl;


        js_wrapped_class.add_base_type(*this);

        cerr << fmt::format("Adding derived bidirectional type {} to base type: {}",
                            js_wrapped_class.class_name, this->name_alias) << endl;

        // set the bidirectional class as being a subclass of the non-bidirectional type
        this->derived_types.insert(&js_wrapped_class);

        js_wrapped_class.include_files.insert("<v8toolkit/bidirectional.h>");
        js_wrapped_class.include_files.insert(js_wrapped_class.my_include);
        cerr << fmt::format("my_include for bidirectional class: {}", js_wrapped_class.my_include) << endl;
    }

    std::cerr << fmt::format("Done creating WrappedClass for {}", this->name_alias) << std::endl;
}


set<unique_ptr<ConstructorFunction>> const & WrappedClass::get_constructors() const {
    assert(this->methods_parsed);
    return this->constructors;
}


set<unique_ptr<MemberFunction>> const & WrappedClass::get_member_functions() const {
    assert(this->methods_parsed);
    return this->member_functions;
}


set<unique_ptr<StaticFunction>> const & WrappedClass::get_static_functions() const {
    assert(this->methods_parsed);
        return this->static_functions;
}


void WrappedClass::parse_all_methods() {


    if (this->methods_parsed || this->decl == nullptr) {
        return;
    }


    this->methods_parsed = true;
    std::cerr << fmt::format("*** Parsing class methods") << std::endl;

    // use decls not methods because methods doesn't give templated functions
    for (Decl * current_decl : this->decl->decls()) {

        if (auto using_shadow_decl = dyn_cast<UsingShadowDecl>(current_decl)) {
            std::cerr << fmt::format("GOT USING SHADOW DECL") << std::endl;
            auto target_decl = using_shadow_decl->getTargetDecl();
            std::cerr << fmt::format("target decl name: {}", target_decl->getNameAsString()) << std::endl;
            std::cerr
                << fmt::format("target decl is cxxmethoddecl? {}", dyn_cast<CXXMethodDecl>(target_decl) != nullptr)
                << std::endl;

            if (dyn_cast<CXXMethodDecl>(target_decl) == nullptr) {
                llvm::report_fatal_error(
                    fmt::format("UsingShadowDecl target decl not a CXXMethodDecl (don't know how to handle this): {}",
                                target_decl->getNameAsString()));
            }
            std::cerr << fmt::format("continuing to parse as if shadow decl target was a method in this class")
                      << std::endl;
            current_decl = target_decl;
        }

        Annotations annotations(current_decl);
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            if (auto named_decl = dyn_cast<NamedDecl>(current_decl)) {
                std::cerr
                    << fmt::format("Skipping {} in {} because V8TOOLKIT_NONE_STRING", named_decl->getNameAsString(),
                                   this->class_name) << std::endl;
            } else {
                std::cerr
                    << fmt::format("Skipping non-named_decl in {} because V8TOOLKIT_NONE_STRING", this->class_name)
                    << std::endl;
            }
        }

        CXXMethodDecl const * method = dyn_cast<CXXMethodDecl>(current_decl);
        map<string, QualType> template_parameter_types;
        FunctionTemplateDecl const * function_template_decl = nullptr;


        if ((function_template_decl = dyn_cast<FunctionTemplateDecl>(current_decl))) {


            method = dyn_cast<CXXMethodDecl>(function_template_decl->getTemplatedDecl());

            if (method == nullptr) {
                llvm::report_fatal_error(fmt::format("FunctionTemplateDecl wasn't a CXXMethodDecl while going through "
                                                         "decl's in {} - not sure what this would mean",
                                                     this->class_name));
            }

            std::string full_method_name(method->getQualifiedNameAsString());

            std::cerr << fmt::format("templated member function: {}", full_method_name) << std::endl;


            if (Annotations(method).has(V8TOOLKIT_NONE_STRING)) {
                std::cerr << fmt::format("SKIPPING TEMPLATE FUNCTION WITH V8TOOLKIT_NONE_STRING") << std::endl;
                continue;
            }



            // store mapping of templated types to default types
            bool all_template_parameters_have_default_value = true;

            std::cerr << fmt::format("num template parameters for function: {}",
                                     function_template_decl->getTemplateParameters()->size()) << std::endl;
            auto template_parameters = function_template_decl->getTemplateParameters();
            for (auto i = template_parameters->begin(); i != template_parameters->end(); i++) {
                std::cerr << fmt::format("template parameter: {}", (*i)->getNameAsString()) << std::endl;

                if (auto template_type_param_decl = dyn_cast<TemplateTypeParmDecl>(*i)) {
                    std::cerr << fmt::format("--is a type parameter") << std::endl;
                    if (template_type_param_decl->hasDefaultArgument()) {
                        auto default_type = template_type_param_decl->getDefaultArgument();
                        std::cerr << fmt::format("----has default argument: {}", get_type_string(default_type))
                                  << std::endl;

                        std::cerr << fmt::format("In template map: {} => {}", (*i)->getNameAsString(),
                                                 default_type.getAsString()) << std::endl;
                        template_parameter_types[(*i)->getNameAsString()] = default_type;
                    } else {
                        all_template_parameters_have_default_value = false;
                    }
                } else if (auto template_value_param_decl = dyn_cast<ValueDecl>(*i)) {
                    std::cerr << fmt::format("--is a value parameter") << std::endl;

                } else {
                    std::cerr << fmt::format("--is unknown type of parameter") << std::endl;
                }
            }
            std::cerr << fmt::format("Do all template parameters have defaults? {}",
                                     all_template_parameters_have_default_value) << std::endl;
            if (!all_template_parameters_have_default_value) {
                continue;

            }
        }

        // if a CXXMethodDecl hasn't been found yet, there's nothing to do for this
        if (!method) {
            continue;
        }


        if (method) {

            Annotations method_annotations(method);

            std::string full_method_name(method->getQualifiedNameAsString());
            cerr << fmt::format("looking at {}", full_method_name) << endl;

            // this is handled now
//            if (method->isTemplateDecl()) {
//                std::cerr << fmt::format("{} is template decl", full_method_name) << std::endl;
//            }

            if (method->hasInheritedPrototype()) {
                cerr << fmt::format("Skipping method %s because it has inherited prototype", full_method_name) << endl;
                continue;
            }

            auto export_type = get_export_type(method, EXPORT_ALL);

            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    printf("Skipping method %s because not supposed to be exported %d\n",
                           full_method_name.c_str(), export_type);
                continue;
            }

            // only deal with public methods
            if (method->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**%s is not public, skipping\n", full_method_name.c_str());
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
                    if (PRINT_SKIPPED_EXPORT_REASONS)
                        printf("**skipping overloaded operator %s\n", full_method_name.c_str());
                    continue;
                }
            }
            if (auto constructor_decl = dyn_cast<CXXConstructorDecl>(method)) {

                // don't deal with constructors on abstract types
                if (this->decl->isAbstract()) {
                    continue;
                }
                if (this->annotations.has(V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS_STRING)) {
                    continue;
                }
                if (this->force_no_constructors) {
                    continue;
                }


                if (constructor_decl->isCopyConstructor()) {
                    fprintf(stderr, "Skipping copy constructor\n");
                    continue;
                } else if (constructor_decl->isMoveConstructor()) {
                    fprintf(stderr, "Skipping move constructor\n");
                    continue;
                } else if (constructor_decl->isDeleted()) {
                    if (print_logging) cerr << "Skipping deleted constructor" << endl;
                    continue;
                }

                // make sure there's no dupes
                auto new_constructor = std::make_unique<ConstructorFunction>(*this, constructor_decl);
                for (auto & existing_constructor : this->constructors) {
                    if (new_constructor->js_name == existing_constructor->js_name) {
                        llvm::report_fatal_error(
                            fmt::format("Duplicate constructor javascript name: {}", new_constructor->js_name));
                    }
                }
                this->constructors.insert(std::move(new_constructor));
                continue;
            }
            if (dyn_cast<CXXDestructorDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping destructor %s\n", full_method_name.c_str());
                continue;
            }
            if (dyn_cast<CXXConversionDecl>(method)) {
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    printf("**skipping conversion operator %s\n", full_method_name.c_str());
                continue;
            }


            if (method_annotations.has(V8TOOLKIT_EXTEND_WRAPPER_STRING)) {
                // cerr << "has extend wrapper string" << endl;
                if (!method->isStatic()) {
                    data_error(fmt::format("method {} annotated with V8TOOLKIT_EXTEND_WRAPPER must be static",
                                           full_method_name.c_str()));

                }
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    cerr << fmt::format(
                        "**skipping static method marked as v8 class wrapper extension method, but will call it during class wrapping")
                         << endl;
                this->wrapper_extension_methods.insert(full_method_name + "(class_wrapper);");
                continue; // don't wrap the method as a normal method
            }

            // this is VERY similar to the one above and both probably aren't needed, but they do allow SLIGHTLY different capabilities
            if (method_annotations.has(V8TOOLKIT_CUSTOM_EXTENSION_STRING)) {
                if (!method->isStatic()) {
                    data_error(fmt::format("method {} annotated with V8TOOLKIT_CUSTOM_EXTENSION must be static",
                                           full_method_name.c_str()));
                }
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    cerr << fmt::format(
                        "**skipping static method marked as V8TOOLKIT_CUSTOM_EXTENSION, but will call it during class wrapping")
                         << endl;
                this->wrapper_custom_extensions.insert(
                    fmt::format("class_wrapper.add_new_constructor_function_template_callback(&{});",
                                full_method_name));
                continue; // don't wrap the method as a normal method
            }

            std::cerr << fmt::format("Creating ParsedMethod...") << std::endl;

            if (method->isStatic()) {
                this->static_functions.insert(
                    make_unique<StaticFunction>(*this, method, template_parameter_types, function_template_decl));
            } else {
                this->member_functions.insert(
                    make_unique<MemberFunction>(*this, method, template_parameter_types, function_template_decl));
            }
        }
    }
}


void WrappedClass::foreach_inheritance_level(function<void(WrappedClass &)> callback) {
    WrappedClass * current_class = this;
    while (true) {

        callback(*current_class);

        if (current_class->base_types.empty()) {
            break;
        }
        current_class = *current_class->base_types.begin();
    }
}


map<string, map<string, int>> const & WrappedClass::get_enums() const {
    assert(this->enums_parsed);
    return this->enums;
};


void WrappedClass::parse_enums() {
    if (this->enums_parsed) {
        return;
    }
    enums_parsed = true;

    if (this->decl == nullptr) {
        std::cerr << fmt::format("No decls for {}", this->name_alias) << std::endl;
        return;
    }

    std::cerr << fmt::format("about to parse decls for enums in {}", this->name_alias) << std::endl;


    for (auto decl : this->decl->decls()) {
        if (auto enum_decl = dyn_cast<EnumDecl>(decl)) {
            if (enum_decl == nullptr) {
                std::cerr << fmt::format("enumdecl is nullptr") << std::endl;
            }
            std::map<std::string, int> enum_class;
            std::cerr << fmt::format("enum name: {}", enum_decl->getNameAsString()) << std::endl;
            for(EnumConstantDecl * constant_decl : enum_decl->enumerators()) {
                std::cerr << fmt::format("enum constant name: {} => {}", constant_decl->getNameAsString(), constant_decl->getInitVal().getExtValue()) << std::endl;
                enum_class[constant_decl->getNameAsString()] = constant_decl->getInitVal().getExtValue();
            }
            this->enums[enum_decl->getNameAsString()] = enum_class;
        }
    }
};


set<unique_ptr<DataMember>> const & WrappedClass::get_members() const {
    assert(this->members_parsed);
    return this->members;
}

void WrappedClass::parse_members() {

    if (this->members_parsed) {
        return;
    }
    this->members_parsed = true;


    this->foreach_inheritance_level([&](WrappedClass & wrapped_class) {
        for (FieldDecl * field : wrapped_class.decl->fields()) {

            string field_name = field->getQualifiedNameAsString();

            auto export_type = get_export_type(field, EXPORT_ALL);
            if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
                if (PRINT_SKIPPED_EXPORT_REASONS)
                    printf("Skipping data member %s because not supposed to be exported %d\n",
                           field_name.c_str(), export_type);
                continue;
            }

            if (field->getAccess() != AS_public) {
                if (PRINT_SKIPPED_EXPORT_REASONS) printf("**%s is not public, skipping\n", field_name.c_str());
                continue;
            }

            this->members.emplace(make_unique<DataMember>(*this, wrapped_class, field));
        }
    });
}


WrappedClass::WrappedClass(const std::string class_name, CompilerInstance & compiler_instance) :
    decl(nullptr),
    class_name(class_name),
    name_alias(class_name),
    compiler_instance(compiler_instance),
    valid(true), // explicitly generated, so must be valid
    found_method(FOUND_GENERATED)
{
}


std::string WrappedClass::generate_js_stub() {
    if (this->name_alias.find("<") != std::string::npos) {
        std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
                                 this->name_alias) << std::endl;
        return std::string();
    } else if (this->base_types.size() > 0 && (*this->base_types.begin())->name_alias.find("<") != std::string::npos) {
        std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
                                 this->name_alias,
                                 (*this->base_types.begin())->name_alias) << std::endl;
        return std::string();
    } else {
        cerr << fmt::format("Generating js stub for {}", this->name_alias) << endl;
    }



    stringstream result;
    string indentation = "    ";

    result << "/**\n";
    result << fmt::format(" * @class {}\n", this->name_alias);

    this->get_enums();
    //    std::cerr << fmt::format("generating stub for {} data members", this->get_members().size()) << std::endl;
    for (auto & member : this->get_members()) {
        result << member->get_js_stub();
    }
    result << fmt::format(" **/\n", indentation);


    result << fmt::format("class {}", this->name_alias);

    if (this->base_types.size() == 1) {
        result << fmt::format(" extends {}", (*this->base_types.begin())->name_alias);
    }
    result << " {\n\n";

    // not sure what to do if there are multiple constructors...
    bool first_method = true;
    for (auto & constructor : this->get_constructors()) {
        if (!first_method) {
            result << ",";
        }
        first_method = false;

        result << endl << endl;
        result << constructor->generate_js_stub();
    }

    std::cerr << fmt::format("generating stub for {} methods", this->get_member_functions().size()) << std::endl;
    for (auto & method : this->get_member_functions()) {
        result << std::endl << method->generate_js_stub() << std::endl;
    }


    std::cerr << fmt::format("generating stub for {} static methods", this->get_static_functions().size()) << std::endl;
    for (auto & method : this->get_static_functions()) {
        result << std::endl << method->generate_js_stub() << std::endl;
    }


    result << fmt::format("\n}}\n");
//    fprintf(stderr, "js stub result for class:\n%s", result.str().c_str());
    return result.str();
}


bool WrappedClass::should_be_wrapped() const {

    cerr << fmt::format("In 'should be wrapped' with class {}, annotations: {}", this->class_name,
                        join(annotations.get())) << endl;

    if (annotations.has(V8TOOLKIT_NONE_STRING) &&
        annotations.has(V8TOOLKIT_ALL_STRING)) {
        cerr << "data error - none and all" << endl;
        data_error(fmt::format("type has both NONE_STRING and ALL_STRING - this makes no sense", class_name));
    }

    if (found_method == FOUND_BASE_CLASS) {
        cerr << fmt::format("should be wrapped {}- found base class (YES)", this->name_alias) << endl;
        return true;
    }
    if (found_method == FOUND_GENERATED) {
        cerr << fmt::format("should be wrapped {}- found generated (YES)", this->name_alias) << endl;
        return true;
    }

    if (found_method == FOUND_INHERITANCE) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            cerr << "Found NONE_STRING" << endl;
            return false;
        }
    } else if (found_method == FOUND_ANNOTATION) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            cerr << "Found NONE_STRING" << endl;
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
            llvm::report_fatal_error(
                fmt::format("Type was supposedly found by annotation, but annotation not found: {}", class_name));
        }
    } else if (found_method == FOUND_UNSPECIFIED) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
            cerr << "Found NONE_STRING on UNSPECIFIED" << endl;
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
            cerr << "didn't find all string on UNSPECIFIED" << endl;
            return false;
        }
        cerr << "FOUND_UNSPECIFIED" << endl;
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
        data_error(
            fmt::format("trying to see if {} should be wrapped but it has more than one base type -- unsupported",
                        class_name));
    }

    cerr << "should be wrapped -- fall through returning true (YES)" << endl;
    return true;
}


bool WrappedClass::ready_for_wrapping(set<WrappedClass *> dumped_classes) const {



    // don't double wrap yourself
    if (find(dumped_classes.begin(), dumped_classes.end(), this) != dumped_classes.end()) {
        printf("Already wrapped %s\n", class_name.c_str());
        return false;
    }

    if (!should_be_wrapped()) {
        cerr << "should be wrapped returned false" << endl;
        return false;
    }

    /*
        // if all this class's directly derived types have been wrapped, then we're good since their
        //   dependencies would have to be met for them to be wrapped
        for (auto derived_type : derived_types) {
            if (find(dumped_classes.begin(), dumped_classes.end(), derived_type) == dumped_classes.end()) {
                printf("Couldn't find %s\n", derived_type->class_name.c_str());
                return false;
            }
        }
    */
    for (auto base_type : base_types) {
        if (find(dumped_classes.begin(), dumped_classes.end(), base_type) == dumped_classes.end()) {
            printf("base type %s not already wrapped - return false\n", base_type->class_name.c_str());
            return false;
        }
    }

    printf("Ready to wrap %s\n", class_name.c_str());

    return true;
}


std::string WrappedClass::get_bindings() {
    stringstream result;
    string indentation = "  ";

    result << indentation << "{\n";
    result << fmt::format("{}  // {}", indentation, class_name) << "\n";
    result << fmt::format("{}  v8toolkit::V8ClassWrapper<{}> & class_wrapper = isolate.wrap_class<{}>();\n",
                          indentation, this->class_name, this->class_name);
    result << fmt::format("{}  class_wrapper.set_class_name(\"{}\");\n", indentation, name_alias);

    for (auto & method : this->get_member_functions()) {
        result << method->generate_js_bindings();
    }

    for (auto & method : this->get_static_functions()) {
        result << method->generate_js_bindings();
    }

    for (auto & member : members) {
        result << member->get_bindings();
    }

    // go through each enum
    for (auto & enumeration : this->get_enums()) {
        std::cerr << fmt::format("writing enum to output file: {}", enumeration.first) << std::endl;
        std::stringstream values;
        values << "{";
        auto first_element = true;

        // go through each value in the enum
        for (auto const & pair : enumeration.second) {
            if (!first_element) {
                values << ", ";
            }
            first_element = false;
            values << "{\"" << pair.first << "\", " << pair.second << "}";
        }
        values << "}";

        result << fmt::format("{}  class_wrapper.add_enum(\"{}\", {});\n", indentation, enumeration.first, values.str());
    };

    for (auto & wrapper_extension_method : wrapper_extension_methods) {
        result << fmt::format("{}  {}\n", indentation, wrapper_extension_method);
    }
    for (auto & wrapper_custom_extension : wrapper_custom_extensions) {
        result << fmt::format("{}  {}\n", indentation, wrapper_custom_extension);
    }

    if (!derived_types.empty()) {
        result << fmt::format("{}  class_wrapper.set_compatible_types<{}>();\n", indentation,
                              get_derived_classes_string());
    }
    if (get_base_class_string() != "") {
        result << fmt::format("{}  class_wrapper.set_parent_type<{}>();\n", indentation,
                              get_base_class_string());
    }
    result << fmt::format("{}  class_wrapper.finalize(true);\n", indentation);

    // if there are no constructors but there are static methods, expose the static methods with this non-constructor name
    if (this->get_constructors().empty() && this->has_static_method()) {
        result
            << fmt::format("{} class_wrapper.expose_static_methods(\"{}\", isolate);\n", indentation, this->name_alias);
    }
        // otherwise just create any constructors that may need wrapping (none is fine)
    else {
        for (auto & constructor : this->get_constructors()) {
            result << constructor->generate_js_bindings();
        }
    }

    result << indentation << "}\n\n";
    return result.str();
}


void WrappedClass::add_member_name(string const & name) {
    // it's ok to have duplicate names, but then this class can not be wrapped
    if (this->used_member_names.count(name) > 0) {
        this->set_error(fmt::format("duplicate name: {}", name));
    }
    this->used_member_names.insert(name);
}

void WrappedClass::add_static_name(string const & name) {
    // it's ok to have duplicate names, but then this class can not be wrapped
    if (this->used_static_names.count(name) > 0) {
        this->set_error(fmt::format("duplicate name: {}", name));
    }
    this->used_static_names.insert(name);
}


void WrappedClass::set_error(string const & error_message) {
    this->data_errors.push_back(error_message);
    this->valid = false;
}


// return all the header files for all the types used by all the base types of the specified type
std::set<string> WrappedClass::get_base_type_includes() const {
    set<string> results{this->my_include};
    results.insert(this->include_files.begin(), this->include_files.end());
    //std::cerr << fmt::format("adding base type include for {} with {} base types", this->class_name, this->base_types.size()) << std::endl;

    //cerr << "Includes at this level: " << endl;
//    for (auto include : results) {
//        cerr << include << endl;
//    }

    for (WrappedClass * base_class : this->base_types) {
        //cerr << fmt::format("...base type: {}", base_class->name_alias) << endl;
        auto base_results = base_class->get_base_type_includes();
        results.insert(base_results.begin(), base_results.end());
    }
    // std::cerr << fmt::format("done adding base type include for {}", this->class_name) << std::endl;

    return results;
}

std::set<string> WrappedClass::get_derived_type_includes() const {
    cerr << fmt::format("Getting derived type includes for {}", name_alias) << endl;
    set<string> results;
    results.insert(my_include);
    for (auto derived_type : derived_types) {

        std::cerr << fmt::format("1 - derived type loop for {}", derived_type->name_alias) << std::endl;
        results.insert(derived_type->include_files.begin(), derived_type->include_files.end());
        //std::cerr << fmt::format("2") << std::endl;
        auto derived_includes = derived_type->get_derived_type_includes();
        //std::cerr << fmt::format("3") << std::endl;
        results.insert(derived_includes.begin(), derived_includes.end());
        //std::cerr << fmt::format("4") << std::endl;
        cerr << fmt::format("{}: Derived type includes for subclass {} and all its derived classes: {}", name_alias,
                            derived_type->class_name, join(derived_includes)) << endl;

    }
    //std::cerr << fmt::format("aaa") << std::endl;
    return results;
}


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


}