
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/Frontend/CompilerInstance.h"
#pragma clang diagnostic pop

#include "wrapped_class.h"
#include "class_handler.h"
#include "helper_functions.h"

#include <xl/library_extensions.h>

namespace v8toolkit::class_parser {


ClassHandler::~ClassHandler() {
    log.info(LogSubjects::Subjects::ClassParser, "ClassHandler destructor");
}

void ClassHandler::run(const ast_matchers::MatchFinder::MatchResult & Result) {

    matched_classes_returned++;
    if (matched_classes_returned % 10000 == 0) {
        std::cerr << fmt::format("\n### Matcher results processed: {}", matched_classes_returned) << std::endl;
    }

    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("other")) {
        std::cerr << fmt::format("OTHER: {}", get_canonical_name_for_decl(klass)) << std::endl;
    }

    // if the current result is matched from the "not std:: class"-bound matcher
    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("not std:: class")) {
        auto class_name = get_canonical_name_for_decl(klass);

        log.info(LogSubjects::ClassParser, "Looking at: {} - anything not filtered", class_name);


        if (klass->isDependentType()) {
            log.info(LogSubjects::ClassParser, "Skipping 'class with annotation' dependent type: {}", class_name);
            return;
        }

        auto name = get_canonical_name_for_decl(klass);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            return;
        }

//        cerr << endl << "Got class definition: " << class_name << endl;
//        fprintf(stderr, "decl ptr: %p\n", (void *) klass);


        if (!is_good_record_decl(klass)) {
            cerr << "SKIPPING BAD RECORD DECL" << endl;
        }

//        cerr << "Storing it for later processing (unless dupe)" << endl;

        log.info(LogSubjects::ClassParser, "class passed checks: {}", class_name);
        WrappedClass::get_or_insert_wrapped_class(klass, FOUND_UNSPECIFIED);
    }

    // Store any annotations on templates so the annotations can be merged later with types instantiated from the template
    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>(
        "forward declaration with annotation")) {

        auto class_name = get_canonical_name_for_decl(klass);
        log.info(LogSubjects::ClassParser, "Looking at: {} - forward declaration with annotation", class_name);


        /* check to see if this has any annotations we should associate with its associated template */
        auto described_tmpl = klass->getDescribedClassTemplate();
        if (klass->isDependentType() && described_tmpl) {
//            fprintf(stderr, "described template %p, %s\n", (void *) described_tmpl, described_tmpl->getQualifiedNameAsString().c_str());
//            printf("Merging %d annotations with template %p\n", (int) Annotations(klass).get().size(), (void *) described_tmpl);
            Annotations::annotations_for_class_templates[described_tmpl].merge(Annotations(klass));
        }
    }


    if (const CXXRecordDecl * klass = Result.Nodes.getNodeAs<clang::CXXRecordDecl>("class derived from WrappedClassBase")) {

        log.info(LogSubjects::ClassParser, "Looking at: {} - class derived from WrappedClassBase", get_canonical_name_for_decl(klass));

        if (!is_good_record_decl(klass)) {
            log.info(LogSubjects::ClassParser, "Skipping 'bad' record decl");
            return;
        }
        if (klass->isDependentType()) {
            log.info(LogSubjects::ClassParser, "skipping dependent type");
            return;
        }

        auto name = get_canonical_name_for_decl(klass);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            std::cerr << fmt::format("Skipping class in std::") << std::endl;
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            std::cerr << fmt::format("skipping class starting with double underscore") << std::endl;
            return;
        }


//        auto class_name = xl::Regex("^(class|struct)?\\s*").replace(get_canonical_name_for_decl(decl), "");
//        auto class_config =
//            PrintFunctionNamesAction::get_config_data()["classes"]
//            [this->class_name];
//
//        if (auto skip = member_function_config["skip"].get_boolean()) {
//            v8toolkit::class_parser::log.info(LogT::Subjects::ConfigFile, "Config file says for {}, skip: {}", field_name, *skip);
//            if (*skip) {
//                continue;
//            } else {
//                // else it was marked as skip = false, so ignore whether it has an annotation or not
//            }
//        } // else no config entry was found, so check for annotation
//        else {
//            if (Annotations(field).has(V8TOOLKIT_NONE_STRING)) {
//                continue;
//            }
//        }

        if (Annotations(klass).has(V8TOOLKIT_NONE_STRING)) {
            log.info(LogSubjects::ClassParser, "Skipping class because it's explicitly marked SKIP");
            return;
        }


//        print_specialization_info(klass);

        log.info(LogSubjects::ClassParser, "Class passed tests: {}", get_canonical_name_for_decl(klass));
        WrappedClass::get_or_insert_wrapped_class(klass, FOUND_INHERITANCE);
    }

    // Store annotations associated with a "using" statement to be merged with the "real" type
    // only pick off the typedefNameDecl entries, but in 3.8, typedefNameDecl() matcher isn't available
    if (auto typedef_decl = Result.Nodes.getNodeAs<clang::TypedefNameDecl>("named decl")) {
        auto qual_type = typedef_decl->getUnderlyingType();
        auto record_decl = qual_type->getAsCXXRecordDecl();

        // not interesting - it's for something like a primitive type like 'long'
        if (!record_decl) {
            log.info(LogSubjects::ClassParser, "skipping because decl wasn't a CXXRecordDecl");
            return;
        }
        auto name = get_canonical_name_for_decl(record_decl);
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?std::.*$"))) {
            log.info(LogSubjects::ClassParser, "Skipping because class in std::");
            return;
        }
        if (std::regex_match(name, regex("^(class\\s+|struct\\s+)?__.*$"))) {
            log.info(LogSubjects::ClassParser, "Skipping class starting with double underscore");
            return;
        }

        Annotations::annotations_for_record_decls[record_decl].merge(Annotations(typedef_decl));

        if (Annotations(typedef_decl).has(V8TOOLKIT_NAME_ALIAS_STRING)) {
            string name_alias = typedef_decl->getNameAsString();
            log.info(LogSubjects::ClassParser, "Annotated type name: {} => {}", record_decl->getQualifiedNameAsString(), typedef_decl->getNameAsString());
            Annotations::names_for_record_decls[record_decl] = name_alias;

            // if the class has already been parsed, update it now
            if (auto wrapped_class = WrappedClass::get_if_exists(record_decl)) {
                log.info(LogSubjects::ClassParser, "Setting typedef name alias for class {} to {}", wrapped_class->class_name, name_alias);
                wrapped_class->force_recache_js_name();
            }
        }
    }

#ifdef TEMPLATE_INFO_ONLY

    if (const ClassTemplateSpecializationDecl * klass = Result.Nodes.getNodeAs<clang::ClassTemplateSpecializationDecl>("class")) {
            auto class_name = get_canonical_name_for_decl(klass);

            bool print_logging = false;

            if (std::regex_search(class_name, std::regex("^(class|struct)?\\s+v8toolkit"))) {
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
                log.info(LogSubjects::ClassParser, "got specialized template ", tmpl->getQualifiedNameAsString());
            }



#ifdef TEMPLATE_FILTER_STD
            if (std::regex_search(tmpl->getQualifiedNameAsString(), std::regex("^std::"))) {
                log.info(LogSubjects::ClassParser, "Skipping class in std::");
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


void ClassHandler::onStartOfTranslationUnit() {
    log.info(LogSubjects::Subjects::ClassParser, "onStartOfTranslationUnit");

    auto & ast_context = this->ci.getASTContext();
    auto & diagnostics_engine = ast_context.getDiagnostics();
    auto diagnostic_consumer = diagnostics_engine.getClient();
    auto error_count = diagnostic_consumer->getNumErrors();
    if (error_count > 0) {
        llvm::report_fatal_error("Errors during compilation, plugin aborting");
    }

}


void ClassHandler::onEndOfTranslationUnit() {

    log.info(LogSubjects::Subjects::ClassParser, "onEndOfTranslationUnit");
    log.info(LogSubjects::Subjects::ClassParser, "Processed total of {} classes from ASTMatchers", matched_classes_returned);

    log.info(LogT::Subjects::ClassParser, ",wrapped classes size: {}", WrappedClass::wrapped_classes.size());

    vector<WrappedClass const *> const should_be_wrapped_classes = [&] {
        vector<WrappedClass const *> results;
        bool found_data_error = false;
        for (auto & c : WrappedClass::wrapped_classes) {
            if (c->should_be_parsed()) {
                c->parse_enums();
                c->parse_members();
                c->parse_all_methods();
                c->validate_data();
                results.push_back(c.get());
            }
            for (auto const & error : c->get_errors()) {

                // actual log.error was published when the error was discovered,
                //   this is just an informational summary of those errors
                log.info(LogT::Subjects::Class, "ERROR SUMMARY: in {}: '{}'", c->class_name, error.string);
                found_data_error = true;
            }
          
        }
        if (found_data_error) {
            
            // this exits the process, which breaks testing
            //llvm::report_fatal_error("Aborting due to data error(s) (search for 'ERROR SUMMARY')");
            throw ClassParserException("Aborting due to data error(s) (search for 'ERROR SUMMARY')");
        }
        return results;
    }();


    if (this->output_modules.empty()) {
        log.warn(LogT::Subjects::ClassParser, "NO OUTPUT MODULES SPECIFIED - did you mean to pass --use-default-output-modules");
        throw ClassParserException("No output modules specified, aborting...");
    }

//    std::cerr << fmt::format("right before processing output modules, log status: {}", log.get_status_string()) << std::endl;
    log.info(LogSubjects::Subjects::ClassParser, "About to run through {} output modules", this->output_modules.size());

    for (auto & output_module : this->output_modules) {

        output_module->_begin();
        log.info(LogSubjects::Subjects::ClassParser, "{} processing", output_module->get_name());

        output_module->process(xl::copy_if(
            should_be_wrapped_classes,
            [&](WrappedClass const * c){return output_module->get_criteria()(*c);}
        ));
        log.info(LogSubjects::Subjects::ClassParser, "{} done processing", output_module->get_name());

        output_module->_end();
        for (auto const & error : output_module->log_watcher.errors) {
            std::cerr << fmt::format("Error during output module: {}: '{}'", output_module->get_name(), error.string) << std::endl;
        }
    }

}




ClassHandler::ClassHandler(CompilerInstance & CI, vector<unique_ptr<OutputModule>> const & output_modules) :
    source_manager(CI.getSourceManager()),
    output_modules(output_modules),
    ci(CI)
{
    log.info(LogSubjects::Subjects::ClassParser, "ClassHandler constructor");
}


} // end namespace v8toolkit::class_parser