#include "wrapped_class.h"
#include "class_handler.h"

namespace v8toolkit::class_parser {


void ClassHandler::run(const ast_matchers::MatchFinder::MatchResult & Result) {

    matched_classes_returned++;

    if (matched_classes_returned % 10000 == 0) {
        std::cerr << fmt::format("\n### Matcher results processed: {}", matched_classes_returned) << std::endl;
    }

    // if the current result is matched from the "not std:: class"-bound matcher
    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("not std:: class")) {
        auto class_name = get_canonical_name_for_decl(klass);

        std::cerr << fmt::format("Got a class that's not a std:: class {}", class_name) << std::endl;


        if (klass->isDependentType()) {
            cerr << "Skipping 'class with annotation' dependent type: " << class_name << endl;
            return;
        }

        auto name = get_canonical_name_for_decl(klass);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            return;
        }

        cerr << endl << "Got class definition: " << class_name << endl;
        fprintf(stderr, "decl ptr: %p\n", (void *) klass);


        if (!is_good_record_decl(klass)) {
            cerr << "SKIPPING BAD RECORD DECL" << endl;
        }

        cerr << "Storing it for later processing (unless dupe)" << endl;

        WrappedClass::get_or_insert_wrapped_class(klass, this->ci, FOUND_UNSPECIFIED);
    }

    // Store any annotations on templates so the annotations can be merged later with types instantiated from the template
    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>(
        "forward declaration with annotation")) {

        auto class_name = get_canonical_name_for_decl(klass);
        cerr << endl << "Got forward declaration with annotation: " << class_name << endl;

        /* check to see if this has any annotations we should associate with its associated template */
        auto described_tmpl = klass->getDescribedClassTemplate();
        if (klass->isDependentType() && described_tmpl) {
            fprintf(stderr, "described template %p, %s\n", (void *) described_tmpl,
                    described_tmpl->getQualifiedNameAsString().c_str());
            printf("Merging %d annotations with template %p\n", (int) Annotations(klass).get().size(),
                   (void *) described_tmpl);
            Annotations::annotations_for_class_templates[described_tmpl].merge(Annotations(klass));
        }
    }


    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>(
        "class derived from WrappedClassBase")) {
        cerr << endl << "Got class derived from v8toolkit::WrappedClassBase: " << get_canonical_name_for_decl(klass)
             << endl;
        if (!is_good_record_decl(klass)) {
            cerr << "skipping 'bad' record decl" << endl;
            return;
        }
        if (klass->isDependentType()) {
            cerr << "skipping dependent type" << endl;
            return;
        }

        auto name = get_canonical_name_for_decl(klass);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            return;
        }


        if (Annotations(klass).has(V8TOOLKIT_NONE_STRING)) {
            cerr << "Skipping class because it's explicitly marked SKIP" << endl;
            return;
        }


        print_specialization_info(klass);


        if (!is_good_record_decl(klass)) {
            cerr << "SKIPPING BAD RECORD DECL" << endl;
        }

        cerr << "Storing it for later processing (unless dupe)" << endl;
        WrappedClass::get_or_insert_wrapped_class(klass, this->ci, FOUND_INHERITANCE);
    }

    // Store annotations associated with a "using" statement to be merged with the "real" type
    // only pick off the typedefNameDecl entries, but in 3.8, typedefNameDecl() matcher isn't available
    if (auto typedef_decl = Result.Nodes.getNodeAs<clang::TypedefNameDecl>("named decl")) {
        auto qual_type = typedef_decl->getUnderlyingType();
        auto record_decl = qual_type->getAsCXXRecordDecl();

        // not interesting - it's for something like a primitive type like 'long'
        if (!record_decl) {
            return;
        }
        auto name = get_canonical_name_for_decl(record_decl);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            return;
        }

        Annotations::annotations_for_record_decls[record_decl].merge(Annotations(typedef_decl));

        if (Annotations(typedef_decl).has(V8TOOLKIT_NAME_ALIAS_STRING)) {
            string name_alias = typedef_decl->getNameAsString();
            std::cerr << fmt::format("Annotated type name: {} => {}", record_decl->getQualifiedNameAsString(),
                                     typedef_decl->getNameAsString()) << std::endl;
            Annotations::names_for_record_decls[record_decl] = name_alias;

            // if the class has already been parsed, update it now
            if (auto wrapped_class = WrappedClass::get_if_exists(record_decl)) {
                wrapped_class->name_alias = name_alias;
            }

        }

    }

#ifdef TEMPLATE_INFO_ONLY

    if (const ClassTemplateSpecializationDecl * klass = Result.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>("class")) {
            auto class_name = get_canonical_name_for_decl(klass);

            bool print_logging = false;

            if (std::regex_search(class_name, std::regex("^(class|struct)\\s+v8toolkit"))) {
            //		if (std::regex_search(class_name, std::regex("remove_reference"))) {
                print_logging = true;
                cerr << fmt::format("Got class {}", class_name) << endl;
            }


#ifdef TEMPLATE_FILTER_STD
            if (std::regex_search(class_name, std::regex("^std::"))) {
                if (print_logging) cerr << "Filtering out because in std::" << endl;
                return;
            }
#endif



            auto tmpl = klass->getSpecializedTemplate();
            if (print_logging) {
                cerr << "got specialized template " << tmpl->getQualifiedNameAsString() << endl;
            }



#ifdef TEMPLATE_FILTER_STD
            if (std::regex_search(tmpl->getQualifiedNameAsString(), std::regex("^std::"))) {
                return;
            }
#endif


            ClassTemplate::get_or_create(tmpl).instantiated();


        }

        if (const CXXMethodDecl * method = Result.Nodes.getNodeAs<clang::CXXMethodDecl>("method")) {
            auto method_name = method->getQualifiedNameAsString();
            const FunctionDecl * pattern = nullptr;

            if (!method->isTemplateInstantiation()) {
                return;
            }
#ifdef TEMPLATE_FILTER_STD
            if (std::regex_search(method_name, std::regex("^std::"))) {
                return;
            }
#endif

            pattern = method->getTemplateInstantiationPattern();
            if (!pattern) {
                pattern = method;
            }

            if (!pattern) {
                llvm::report_fatal_error("method is template insantiation but pattern still nullptr");
            }

            FunctionTemplate::get_or_create(pattern).instantiated();


#if 0
            bool print_logging = false;

            if (std::regex_search(method_name, std::regex("function_in_temp"))) {
                cerr << endl << "*******Found function in templated class decl" << endl;
                fprintf(stderr, "Method decl ptr: %p\n", (void*) method);
                cerr << "is dependent context: " << method->isDependentContext() << endl;
                cerr << "has dependent template info: " << (method->getDependentSpecializationInfo() != nullptr) << endl;
                cerr << "is template instantiation: " << (method->isTemplateInstantiation()) << endl;
                cerr << "has instantiation pattern: " << (method->getTemplateInstantiationPattern() != nullptr) << endl;
                if (method->getTemplateInstantiationPattern()) {
                fprintf(stderr, "template instantiation pattern ptr: %p\n", (void*) method->getTemplateInstantiationPattern());
                }
                print_logging = true;
            }

            const FunctionTemplateDecl * function_template_decl = method->getDescribedFunctionTemplate();

            if (function_template_decl == nullptr && method->getTemplateSpecializationInfo()) {
                function_template_decl = method->getTemplateSpecializationInfo()->getTemplate();
            }

            if (function_template_decl) {
                cerr << fmt::format("'real' templated method {} has instantiation pattern: {}", method_name, method->getTemplateInstantiationPattern() != nullptr) << endl;
                fprintf(stderr, "method: %p, instantiation pattern: %p\n", (void *)method, (void*)method->getTemplateInstantiationPattern());
                if (print_logging)
                cerr << fmt::format("Got method {}", method_name) << endl;
                FunctionTemplate::get_or_create(function_template_decl).instantiated();
            } else {
                if (print_logging) cerr << "not interesting method" << endl;
            }
            return;

#endif

        }
#endif // end TEMPLATE_INFO_ONLY
}


void ClassHandler::onEndOfTranslationUnit() {

    for (auto & warning : data_warnings) {
        cerr << warning << endl;
    }


    if (!data_errors.empty()) {
        cerr << "Errors detected:" << endl;
        for (auto & error : data_errors) {
            cerr << error << endl;
        }
        llvm::report_fatal_error("Errors detected in source data");
        exit(1);
    }


    cerr << "*************" << endl << "ABOUT TO GENERATE OUTPUT FILES" << endl << "*****************" << endl;

    generate_javascript_stub("js-api.js");
    generate_bidirectional_classes(this->ci);
    generate_bindings();
}


#if 0
void ClassHandler::handle_method(WrappedClass & klass, CXXMethodDecl * method) {


    std::string full_method_name(method->getQualifiedNameAsString());
    std::string short_method_name(method->getNameAsString());

    Annotations annotations(method);




    if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("Handling method: {}", full_method_name) << endl;
    //            if (print_logging) cerr << "changing method name from " << full_method_name << " to ";
//
//            auto regex = std::regex(fmt::format("{}::{}$", containing_class->getName().str(), short_method_name));
//            auto replacement = fmt::format("{}::{}", top_level_class_decl->getName().str(), short_method_name);
//            full_method_name = std::regex_replace(full_method_name, regex, replacement);
//            if (print_logging) cerr << full_method_name << endl;


    auto export_type = get_export_type(method, EXPORT_ALL);

    if (export_type != EXPORT_ALL && export_type != EXPORT_EXCEPT) {
        if (PRINT_SKIPPED_EXPORT_REASONS) printf("Skipping method %s because not supposed to be exported %d\n",
                                                 full_method_name.c_str(), export_type);
        return;
    }

    // only deal with public methods
    if (method->getAccess() != AS_public) {
        if (PRINT_SKIPPED_EXPORT_REASONS) printf("**%s is not public, skipping\n", full_method_name.c_str());
        return;
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
            return;
        }
    }
    if (dyn_cast<CXXConstructorDecl>(method)) {
        if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping constructor %s\n", full_method_name.c_str());
        return;
    }
    if (dyn_cast<CXXDestructorDecl>(method)) {
        if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping destructor %s\n", full_method_name.c_str());
        return;
    }
    // still want to keep the interface even if it's not implemented here
    // if (method->isPure()) {
    //     if(!method->isVirtual()) {
    // 	    llvm::report_fatal_error("Got pure non-virtual method - not sure what that even means", false);
    // 	}
    //     if (PRINT_SKIPPED_EXPORT_REASONS) printf("%s**skipping pure virtual %s\n", indentation.c_str(), full_method_name.c_str());
    //     return;
    // }


    // If the function is wrapped in derived classes as well, you run into problems where you can't find the right type to
    //   cast the internal field to to find a match for the function type.   You may only get a Base* when you need to call void(Derived::*)()
    //   so if you only have the virtual function wrapped in Base, you'll always find the right type of object
//            if (method->isVirtual()) {
//                fprintf(stderr, "%s :: %s is virtual with %d overrides\n", klass.class_name.c_str(), full_method_name.c_str(), (int)method->size_overridden_methods());
//            } else {
//                fprintf(stderr, "%s :: %s isn't virtual\n", klass.class_name.c_str(), full_method_name.c_str());
//            }
    if (method->isVirtual() && method->size_overridden_methods()) {
        if (PRINT_SKIPPED_EXPORT_REASONS) printf("**skipping derived-class override of base class virtual function %s\n", full_method_name.c_str());
        return;
    }

    if (dyn_cast<CXXConversionDecl>(method)) {
        if (PRINT_SKIPPED_EXPORT_REASONS) cerr << fmt::format("**skipping user-defined conversion operator") << endl;
        return;
    }
    if (PRINT_SKIPPED_EXPORT_REASONS) cerr << "Method passed all checks" << endl;






    //	    cerr << "Checking if method name already used" << endl;
    if (top_level_class->names.count(short_method_name)) {
        data_error(fmt::format("Skipping duplicate name {}/{} :: {}\n",
                               top_level_class->class_name,
                               klass.class_name,
                               short_method_name));
        return;
    }
    //cerr << "Inserting short name" << endl;
    top_level_class->names.insert(short_method_name);

    auto parsed_method = make_unique<ParsedMethod>(this->ci, klass, method);

} // end handle_method

#endif


#if 0
std::string ClassHandler::handle_data_member(WrappedClass & containing_class, FieldDecl * field, const std::string & indentation) {
    std::stringstream result;
    auto export_type = get_export_type(field, EXPORT_ALL);
    auto short_field_name = field->getNameAsString();
    auto full_field_name = field->getQualifiedNameAsString();


    cerr << "Processing data member for: " << containing_class.name_alias << ": " << full_field_name << endl;
//            if (containing_class != top_level_class_decl) {
//                if (print_logging) cerr << "************";
//            }
//            if (print_logging) cerr << "changing data member from " << full_field_name << " to ";
//
//            std::string regex_string = fmt::format("{}::{}$", containing_class->getName().str(), short_field_name);
//            auto regex = std::regex(regex_string);
//            auto replacement = fmt::format("{}::{}", top_level_class_decl->getName().str(), short_field_name);
//            full_field_name = std::regex_replace(full_field_name, regex, replacement);
//            if (print_logging) cerr << full_field_name << endl;


    top_level_class->names.insert(short_field_name);


    // made up number to represent the overhead of making a new wrapped class
    //   even before adding methods/members
    // This means that two wrapped classes will count as much towards rolling to the next file as
    // one wrapped class with <THIS NUMBER> of wrapped members/functions

    update_wrapped_class_for_type(ci, *top_level_class, field->getType());

    string full_type_name = get_type_string(field->getType());

    std::cerr << fmt::format("incrementing declaration count for {} - data member", top_level_class->name_alias) << std::endl;


//            printf("%sData member %s, type: %s\n",
//                   indentation.c_str(),
//                   field->getNameAsString().c_str(),
//                   field->getType().getAsString().c_str());
    return result.str();
}
#endif

}