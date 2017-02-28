
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
    std::cerr << fmt::format("*** Creating WrappedClass for {}", this->name_alias) << std::endl;
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


    std::cerr << fmt::format("*** Parsing class methods") << std::endl;
    for (CXXMethodDecl * method : this->decl->methods()) {

        std::string full_method_name(method->getQualifiedNameAsString());

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


    for (FieldDecl * field : this->decl->fields()) {

        string field_name = field->getQualifiedNameAsString();

        auto export_type = get_export_type(field, EXPORT_ALL);
        if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
            if (PRINT_SKIPPED_EXPORT_REASONS) printf("Skipping data member %s because not supposed to be exported %d\n",
                                                     field_name.c_str(), export_type);
            continue;
        }

        if (field->getAccess() != AS_public) {
            if (PRINT_SKIPPED_EXPORT_REASONS) printf("**%s is not public, skipping\n", field_name.c_str());
            continue;
        }



        auto data_member = make_unique<DataMember>(*this, field);

        if (this->names.count(data_member->short_name)) {
            data_error(fmt::format("ERROR: duplicate name {} :: {}\n",
                                   this->class_name,
                                   data_member->short_name));
            continue;
        }
        this->names.insert(data_member->short_name);
        this->members.emplace(std::move(data_member));



    }


    cerr << "Final wrapped class annotations: " << endl;
    print_vector(annotations.get());

    std::cerr << fmt::format("Done creating WrappedClass for {}", this->name_alias) << std::endl;
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

    //    std::cerr << fmt::format("generating stub for {} methods", this->methods.size()) << std::endl;
    for (auto & method : this->methods) {
        result << method->get_js_stub();
    }


    result << fmt::format("}}\n\n\n");
//    fprintf(stderr, "js stub result for class:\n%s", result.str().c_str());
    return result.str();
}



bool WrappedClass::should_be_wrapped() const {
    auto a = class_name;
    auto b = found_method;
    auto c = join(annotations.get());
//        cerr << fmt::format("In 'should be wrapped' with class {}, found_method: {}, annotations: {}", a, b, c) << endl;

    if (annotations.has(V8TOOLKIT_NONE_STRING) &&
        annotations.has(V8TOOLKIT_ALL_STRING)) {
        data_error(fmt::format("type has both NONE_STRING and ALL_STRING - this makes no sense", class_name));
    }

    if (found_method == FOUND_BASE_CLASS) {
        return true;
    }
    if (found_method == FOUND_GENERATED) {
        return true;
    }

    if (found_method == FOUND_INHERITANCE) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
//            cerr << "Found NONE_STRING" << endl;
            return false;
        }
    } else if (found_method == FOUND_ANNOTATION) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
//            cerr << "Found NONE_STRING" << endl;
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
            llvm::report_fatal_error(fmt::format("Type was supposedly found by annotation, but annotation not found: {}", class_name));
        }
    } else if (found_method == FOUND_UNSPECIFIED) {
        if (annotations.has(V8TOOLKIT_NONE_STRING)) {
//            cerr << "Found NONE_STRING on UNSPECIFIED" << endl;
            return false;
        }
        if (!annotations.has(V8TOOLKIT_ALL_STRING)) {
//            cerr << "didn't find all string on UNSPECIFIED" << endl;
            return false;
        }
    }

    if (base_types.size() > 1) {
        data_error(fmt::format("trying to see if type should be wrapped but it has more than one base type -- unsupported", class_name));
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
                          indentation, class_name, class_name);
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