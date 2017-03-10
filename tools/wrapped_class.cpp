
#include "wrapped_class.h"
#include "parsed_method.h"
#include "class_handler.h"

WrappedClass::WrappedClass(const CXXRecordDecl * decl, CompilerInstance & compiler_instance, FOUND_METHOD found_method) :
    decl(decl),
    class_name(get_canonical_name_for_decl(decl)),
    name_alias(decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString()),
    compiler_instance(compiler_instance),
    my_include(get_include_for_type_decl(compiler_instance, decl)),
    annotations(decl),
    found_method(found_method)
{

    std::cerr << fmt::format("*** Creating WrappedClass for {} with found_method = {}", this->name_alias, this->found_method) << std::endl;
    fprintf(stderr, "Creating WrappedClass for record decl ptr: %p\n", (void *)decl);
    string using_name = Annotations::names_for_record_decls[decl];
    if (!using_name.empty()) {
        cerr << fmt::format("Setting name alias for {} to {} because of a 'using' statement", class_name, using_name) << endl;
        name_alias = using_name;
    }

    // strip off any leading "class " or "struct " off the type name
    name_alias = regex_replace(name_alias, std::regex("^(struct|class) "), "");


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


    update_wrapped_class_for_type(*this,
                                  this->decl->getTypeForDecl()->getCanonicalTypeInternal());


    const ClassTemplateSpecializationDecl *specialization = nullptr;
    if ((specialization = dyn_cast<ClassTemplateSpecializationDecl>(this->decl)) != nullptr) {
        auto specialized_template = specialization->getSpecializedTemplate();
        auto template_name = specialized_template->getNameAsString();
        template_instantiations[template_name]++;
    }

    cerr << "Final wrapped class annotations: " << endl;
    print_vector(annotations.get());



    // START NEW CODE
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
        WrappedClass &current_base = WrappedClass::get_or_insert_wrapped_class(base_record_decl, this->compiler_instance,
                                                                               this->found_method_means_wrapped() ? FOUND_BASE_CLASS : FOUND_UNSPECIFIED);


        auto current_base_include = get_include_for_type_decl(this->compiler_instance, current_base.decl);
        auto current_include = get_include_for_type_decl(this->compiler_instance, this->decl);
        //                printf("For %s, include %s -- for %s, include %s\n", current_base->class_name.c_str(), current_base_include.c_str(), current->class_name.c_str(), current_include.c_str());

        this->include_files.insert(current_base_include);
        current_base.include_files.insert(current_include);
        this->base_types.insert(&current_base);
        current_base.derived_types.insert(this);

        //printf("%s now has %d base classes\n", current->class_name.c_str(), (int)current->base_types.size());
        //printf("%s now has %d derived classes\n", current_base->class_name.c_str(), (int)current_base->derived_types.size());


    }

    if (print_logging) cerr << "done with base classes" << endl;
    if (must_have_base_type && !found_base_type) {
        data_error(
            fmt::format("base_type_to_use specified but no base type found: {}", this->class_name));
    }




    // END NEW CODE


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
            this->set_error(fmt::format("Bidirectional class {} doesn't have a bidirectional constructor explicitly set", this->name_alias));
        }

        string bidirectional_class_name = fmt::format("JS{}", this->name_alias);

        // created a WrappedClass for the non-AST JSWrapper class
        WrappedClass * js_wrapped_class = new WrappedClass(bidirectional_class_name,
                                            this->compiler_instance);
        js_wrapped_class->bidirectional = true;
        js_wrapped_class->my_include = fmt::format("\"v8toolkit_generated_bidirectional_{}.h\"", this->name_alias);
        cerr << fmt::format("my_include for bidirectional class: {}" , js_wrapped_class->my_include) << endl;

        WrappedClass::insert_wrapped_class(js_wrapped_class);

        js_wrapped_class->base_types.insert(this);

        cerr << fmt::format("Adding derived bidirectional type {} to base type: {}",
                            js_wrapped_class->class_name, this->name_alias) << endl;

        // set the bidirectional class as being a subclass of the non-bidirectional type
        this->derived_types.insert(js_wrapped_class);

        //js_wrapped_class->include_files.insert(js_wrapped_class->my_header_filename);
        cerr << fmt::format("my_include for bidirectional class: {}" , js_wrapped_class->my_include) << endl;
    }

    std::cerr << fmt::format("Done creating WrappedClass for {}", this->name_alias) << std::endl;
}



set<unique_ptr<ParsedMethod>> & WrappedClass::get_methods() {
    if (this->decl == nullptr) {
        llvm::report_fatal_error(fmt::format("Can't get_methods on type without decl: {}", this->name_alias).c_str());
    }
    if (this->methods_parsed) {
        return this->methods;
    }

    this->methods_parsed = true;
     std::cerr << fmt::format("*** Parsing class methods") << std::endl;
    for (CXXMethodDecl * method : this->decl->methods()) {

        std::string full_method_name(method->getQualifiedNameAsString());
        //cerr << fmt::format("looking at {}", full_method_name) << endl;

        if (method->hasInheritedPrototype()) {
            cerr << fmt::format("Skipping method because it has inherited prototype"
                                /*, method->getNameAsString()  - this crashes due to "not simple identifier" */) << endl;
            continue;
        }

        auto export_type = get_export_type(method, EXPORT_ALL);

        if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
            if (PRINT_SKIPPED_EXPORT_REASONS) printf("Skipping method %s because not supposed to be exported %d\n",
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
        if (dyn_cast<CXXConstructorDecl>(method)) {
            if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping constructor %s\n", full_method_name.c_str());
            continue;
        }
        if (dyn_cast<CXXDestructorDecl>(method)) {
            if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping destructor %s\n", full_method_name.c_str());
            continue;
        }

        std::cerr << fmt::format("Creating ParsedMethod...") << std::endl;
        this->methods.insert(make_unique<ParsedMethod>(this->compiler_instance, *this, method));
    }
    return this->methods;
}


set<unique_ptr<DataMember>> & WrappedClass::get_members() {
    if (this->members_parsed) {
        return this->members;
    }
    this->members_parsed = true;
    for (FieldDecl * field : this->decl->fields()) {

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

        this->members.emplace(make_unique<DataMember>(*this, field));
    }
    return this->members;
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

    cerr << fmt::format("Generating js stub for {}", this->name_alias) << endl;

    stringstream result;
    string indentation = "    ";

    result << "/**\n";
    result << fmt::format(" * @class {}\n", this->name_alias);

    //    std::cerr << fmt::format("generating stub for {} data members", this->members.size()) << std::endl;
    for (auto & member : this->members) {
        result << member->get_js_stub();
    }
    result << fmt::format(" **/\n", indentation);


    result << fmt::format("class {}", this->name_alias);

    if (this->base_types.size() == 1) {
        result << fmt::format(" extends {}", (*this->base_types.begin())->name_alias);
    }
    result << "{\n";

    CXXFinalOverriderMap override_map;
    this->decl->getFinalOverriders(override_map);

    //    std::cerr << fmt::format("generating stub for {} methods", this->methods.size()) << std::endl;
    for (auto & method : this->methods) {

        result << method->get_js_stub();
    }


    result << fmt::format("}}\n\n\n");
//    fprintf(stderr, "js stub result for class:\n%s", result.str().c_str());
    return result.str();
}



bool WrappedClass::should_be_wrapped() const {

    cerr << fmt::format("In 'should be wrapped' with class {}, annotations: {}", this->class_name, join(annotations.get())) << endl;

    if (annotations.has(V8TOOLKIT_NONE_STRING) &&
        annotations.has(V8TOOLKIT_ALL_STRING)) {
        cerr << "data error - none and all" << endl;
        data_error(fmt::format("type has both NONE_STRING and ALL_STRING - this makes no sense", class_name));
    }

    if (found_method == FOUND_BASE_CLASS) {
        cerr << fmt::format("should be wrapped {}- found base class", this->name_alias) << endl;
        return true;
    }
    if (found_method == FOUND_GENERATED) {
        cerr << fmt::format("should be wrapped {}- found generated", this->name_alias) << endl;
        return false;
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
            llvm::report_fatal_error(fmt::format("Type was supposedly found by annotation, but annotation not found: {}", class_name));
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
        data_error(fmt::format("trying to see if {} should be wrapped but it has more than one base type -- unsupported", class_name));
    }

    cerr << "should be wrapped -- fall through returning true" << endl;
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



std::string WrappedClass::get_bindings(){
    stringstream result;
    string indentation = "  ";

    result << indentation << "{\n";
    result << fmt::format("{}  // {}", indentation, class_name) << "\n";
    result << fmt::format("{}  v8toolkit::V8ClassWrapper<{}> & class_wrapper = isolate.wrap_class<{}>();\n",
                          indentation, name_alias, name_alias);
    result << fmt::format("{}  class_wrapper.set_class_name(\"{}\");\n", indentation, name_alias);

    for(auto & method : methods) {
        result << method->get_bindings();
    }
    for(auto & member : members) {
        result << member->get_bindings();
    }
    for(auto & wrapper_extension_method : wrapper_extension_methods) {
        result << fmt::format("{}  {}\n", indentation, wrapper_extension_method);
    }
    for(auto & wrapper_custom_extension : wrapper_custom_extensions) {
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

    for(auto & constructor : constructors) {
        result << constructor;
    }

    result << indentation << "}\n\n";
    return result.str();
}


void WrappedClass::add_name(string const & name) {
    // it's ok to have duplicate names, but then this class can not be wrapped
    if (this->used_names.count(name) > 0) {
        this->set_error(fmt::format("duplicate name: {}", name));
    }
    this->used_names.insert(name);
}




void WrappedClass::set_error(string const & error_message) {
    this->data_errors.push_back(error_message);
    this->valid = false;
}



// return all the header files for all the types used by all the base types of the specified type
std::set<string> WrappedClass::get_base_type_includes() {
    set<string> results{this->my_include};
    results.insert(this->include_files.begin(), this->include_files.end());
    std::cerr << fmt::format("adding base type include for {} with {} base types", this->class_name, this->base_types.size()) << std::endl;

    cerr << "Includes at this level: " << endl;
    for (auto include : results) {
        cerr << include << endl;
    }

    for (WrappedClass * base_class : this->base_types) {
        cerr << fmt::format("...base type: {}", base_class->name_alias) << endl;
        auto base_results = base_class->get_base_type_includes();
        results.insert(base_results.begin(), base_results.end());
    }
    std::cerr << fmt::format("done adding base type include for {}", this->class_name) << std::endl;

    return results;
}

std::set<string> WrappedClass::get_derived_type_includes() {
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
        cerr << fmt::format("{}: Derived type includes for subclass {} and all its derived classes: {}", name_alias, derived_type->class_name, join(derived_includes)) << endl;

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
