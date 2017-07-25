#include "ast_action.h"

namespace v8toolkit::class_parser {


static FrontendPluginRegistry::Add <PrintFunctionNamesAction>
    X("v8toolkit-generate-bindings", "generate v8toolkit bindings");

// This is called when all parsing is done
void PrintFunctionNamesAction::EndSourceFileAction() {

}

// takes a file number starting at 1 and incrementing 1 each time
// a list of WrappedClasses to print
// and whether or not this is the last file to be written
void write_classes(int file_count, vector<WrappedClass *> & classes, bool last_one) {

    cerr << fmt::format("writing classes, file_count: {}, classes.size: {}, last_one: {}", file_count, classes.size(),
                        last_one) << endl;
    // Open file
    string class_wrapper_filename = fmt::format("v8toolkit_generated_class_wrapper_{}.cpp", file_count);
    ofstream class_wrapper_file;

    class_wrapper_file.open(class_wrapper_filename, ios::out);
    if (!class_wrapper_file) {
        if (print_logging) cerr << "Couldn't open " << class_wrapper_filename << endl;
        throw std::exception();
    }

    // by putting in some "fake" includes, it will stop these from ever being put in since it will
    //   think they already are, even though they really aren't
    set<string> already_included_this_file;
    already_included_this_file.insert(never_include_for_any_file.begin(), never_include_for_any_file.end());

    class_wrapper_file << header_for_every_class_wrapper_file << "\n";

    // first the ones that go in every file regardless of its contents
    for (auto & include : includes_for_every_class_wrapper_file) {
        class_wrapper_file << fmt::format("#include {}\n", include);
        already_included_this_file.insert(include);
    }


    std::set<WrappedClass *> extern_templates;
    set<string> includes_for_this_file;

    for (WrappedClass * wrapped_class : classes) {

        // force methods to be parsed
        wrapped_class->parse_all_methods();

        for (auto derived_type : wrapped_class->derived_types) {
            extern_templates.insert(derived_type);
        }

        for (auto used_class : wrapped_class->used_classes) {
            if (used_class->should_be_wrapped()) {
                extern_templates.insert(used_class);
            }
        }


        if (print_logging)
            cerr << "Dumping " << wrapped_class->class_name << " to file " << class_wrapper_filename << endl;

        //                printf("While dumping classes to file, %s has includes: ", wrapped_class->class_name.c_str());

        // Combine the includes needed for types in members/methods with the includes for the wrapped class's
        //   derived types
        //std::cerr << fmt::format("aa1") << std::endl;
        //std::cerr << fmt::format("aa2") << std::endl;

        auto base_type_includes = wrapped_class->get_base_type_includes();
        includes_for_this_file.insert(base_type_includes.begin(), base_type_includes.end());

        auto derived_type_includes = wrapped_class->get_derived_type_includes();
        includes_for_this_file.insert(derived_type_includes.begin(), derived_type_includes.end());


    } // end loop through all classes for this file


    for (auto & include_file : includes_for_this_file) {
        if (include_file != "") {
            // skip "internal looking" includes - look at 1 because 0 is < or "
            if (include_file.find("__") == 1) {
                continue;
            }
            //std::cerr << fmt::format("aa5") << std::endl;
            class_wrapper_file << fmt::format("#include {}\n", include_file);
        }
    }


//std::cerr << fmt::format("aa7") << std::endl;
    // remove any types that are wrapped in this file since it will be explicitly instantiated here
    for (auto & wrapped_class : classes) {
//::cerr << fmt::format("aa8") << std::endl;
        // DO NOT EXPLICITLY INSTANTIATE THE WRAPPED TYPE
//		if (wrapped_class->is_template_specialization()) {
//		    class_wrapper_file << "template " << wrapped_class->class_name << ";" << endl;
//		}
        class_wrapper_file << fmt::format("template class v8toolkit::V8ClassWrapper<{}>;\n", wrapped_class->class_name);

        // the const type will be used by the 'third party' extension function, so it needs to be instantiated
        if (!wrapped_class->wrapper_extension_methods.empty()) {
            class_wrapper_file
                << fmt::format("template class v8toolkit::V8ClassWrapper<{} const>;\n", wrapped_class->class_name);
        }


        // if it's not a template specialization it shouldn't be in the extern_template set, but delete it anyhow
        extern_templates.erase(wrapped_class);
        //std::cerr << fmt::format("aa7.5") << std::endl;
    }
    //std::cerr << fmt::format("aa8") << std::endl;

    for (auto extern_template : extern_templates) {
        //std::cerr << fmt::format("aa9 {}", extern_template->name_alias) << std::endl;
        if (extern_template->is_template_specialization()) {
            //std::cerr << fmt::format("aa10") << std::endl;
            class_wrapper_file << "extern template " << extern_template->class_name << ";\n";
        }
        //std::cerr << fmt::format("aa11") << std::endl;
        class_wrapper_file
            << fmt::format("extern template class v8toolkit::V8ClassWrapper<{}>;\n", extern_template->class_name);
    }
    //std::cerr << fmt::format("aa12") << std::endl;



    // Write function header
    class_wrapper_file << fmt::format(
        "void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate &); // may not exist -- that's ok\n",
        file_count + 1);
    //std::cerr << fmt::format("aa13") << std::endl;

    if (file_count == 1) {
        class_wrapper_file
            << fmt::format("void v8toolkit_initialize_class_wrappers(v8toolkit::Isolate & isolate) {{\n");

    } else {
        class_wrapper_file
            << fmt::format("void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate & isolate) {{\n",
                           file_count);
    }


    //std::cerr << fmt::format("aa14") << std::endl;
    // Print function body
    for (auto wrapped_class : classes) {
        //std::cerr << fmt::format("aa15") << std::endl;
        // each file is responsible for making explicit instantiatinos of its own types
        class_wrapper_file << wrapped_class->get_bindings();
    }
    //std::cerr << fmt::format("aa16") << std::endl;

    // if there's going to be another file, call the function in it
    if (!last_one) {
        class_wrapper_file << fmt::format("  v8toolkit_initialize_class_wrappers_{}(isolate);\n", file_count + 1);
    }

    // Close function and file
    class_wrapper_file << "}\n";
    class_wrapper_file.close();

}

}