
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

    for (auto field : this->fields) {
        ParsedMethod::TypeInfo field_type{field->getType()};
        result << fmt::format(" * @property {{{}}} {} \n", field_type.jsdoc_type_name, field_type.name);
    }
    result << fmt::format(" **/\n", indentation);


    result << fmt::format("class {}", this->name_alias);

    if (this->base_types.size() == 1) {
        result << fmt::format(" extends {}", (*this->base_types.begin())->name_alias);
    }
    result << "{\n";

    std::cerr << fmt::format("generating stub for {} methods", this->methods.size()) << std::endl;
    for (auto & method : this->methods) {
        result << method->get_wrapper_string();
    }


    result << fmt::format("}}\n\n\n");
    fprintf(stderr, "js stub result for class:\n%s", result.str().c_str());
    return result.str();
}
