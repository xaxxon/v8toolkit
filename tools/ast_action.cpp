#include "ast_action.h"

static FrontendPluginRegistry::Add<PrintFunctionNamesAction>
    X("v8toolkit-generate-bindings", "generate v8toolkit bindings");

// This is called when all parsing is done
void PrintFunctionNamesAction::EndSourceFileAction() {


#ifdef TEMPLATE_INFO_ONLY
    {
		cerr << fmt::format("Class template instantiations") << endl;
		vector<pair<string, int>> insts;
		for (auto & class_template : class_templates) {
		    insts.push_back({class_template->name, class_template->instantiations});
		}
		std::sort(insts.begin(), insts.end(), [](auto & a, auto & b){
			return a.second < b.second;
		    });
		int skipped = 0;
		int total = 0;
		cerr << endl << fmt::format("Class templates with more than {} or more instantiations:", TEMPLATED_CLASS_PRINT_THRESHOLD) << endl;
		for (auto & pair : insts) {
		    total += pair.second;
		    if (pair.second < TEMPLATED_CLASS_PRINT_THRESHOLD) {
		    skipped++;
		    continue;
		    }
		    cerr << pair.first << ": " << pair.second << endl;;
		}
		cerr << endl;
		cerr << "Skipped " << skipped << " entries because they had fewer than " << TEMPLATED_CLASS_PRINT_THRESHOLD << " instantiations" << endl;
		cerr << "Total of " << total << " instantiations" << endl;
		skipped = 0;
		total = 0;
		insts.clear();
		for (auto & function_template : function_templates) {
		    insts.push_back({function_template->name, function_template->instantiations});
		}
		std::sort(insts.begin(), insts.end(), [](auto & a, auto & b){
			return a.second < b.second;
		    });
		cerr << endl << fmt::format("Function templates with more than {} or more instantiations:", TEMPLATED_FUNCTION_PRINT_THRESHOLD) << endl;
		for (auto & pair : insts) {
		    total += pair.second;
		    if (pair.second < TEMPLATED_FUNCTION_PRINT_THRESHOLD) {
			skipped++;
			continue;
		    }
		    cerr << pair.first << ": " << pair.second << endl;;
		}


		cerr << endl;
		cerr << "Skipped " << skipped << " entries because they had fewer than " << TEMPLATED_FUNCTION_PRINT_THRESHOLD << " instantiations" << endl;
		cerr << "Total of " << total << " instantiations" << endl;
		return;
	    }
#endif


    for (auto & warning : data_warnings) {
        cerr << warning << endl;
    }



    if (!data_errors.empty()) {
        cerr << "Errors detected:" << endl;
        for(auto & error : data_errors) {
            cerr << error << endl;
        }
        llvm::report_fatal_error("Errors detected in source data");
        exit(1);
    }

    // Write class wrapper data to a file
    int file_count = 1;

    // start at one so they are never considered "empty"
    int declaration_count_this_file = 1;
    vector<WrappedClass*> classes_for_this_file;

    set<WrappedClass *> already_wrapped_classes;

    vector<vector<WrappedClass *>> classes_per_file;

    ofstream js_stub;



    cerr << fmt::format("About to start writing out wrapped classes with {} potential classes", WrappedClass::wrapped_classes.size()) << endl;

    bool found_match = true;
    while (found_match) {
        found_match = false;

        for (auto wrapped_class_iterator = WrappedClass::wrapped_classes.begin();
             wrapped_class_iterator != WrappedClass::wrapped_classes.end();
             wrapped_class_iterator++) {

            WrappedClass &wrapped_class = **wrapped_class_iterator;

            if (!wrapped_class.valid) {
                //			cerr << "Skipping 'invalid' class: " << wrapped_class.class_name << endl;
                continue;
            }

            cerr << fmt::format("considering dumping class: {}", wrapped_class.class_name) << endl;

            // if it has unmet dependencies or has already been mapped, skip it
            if (!wrapped_class.ready_for_wrapping(already_wrapped_classes)) {
                printf("Skipping %s\n", wrapped_class.class_name.c_str());
                continue;
            }
            already_wrapped_classes.insert(&wrapped_class);
            found_match = true;

            std::cerr << fmt::format("writing class {} to file with declaration_count = {}", wrapped_class.name_alias, wrapped_class.declaration_count) << std::endl;

            // if there's room in the current file, add this class
            auto space_available = declaration_count_this_file == 0 ||
                                   declaration_count_this_file + wrapped_class.declaration_count <
                                   MAX_DECLARATIONS_PER_FILE;

            if (!space_available) {


                std::cerr << fmt::format("Out of space in file, rotating") << std::endl;
                classes_per_file.emplace_back(classes_for_this_file);

                // reset for next file
                classes_for_this_file.clear();
                declaration_count_this_file = 0;
                file_count++;
            }

            classes_for_this_file.push_back(&wrapped_class);
            wrapped_class.dumped = true;
            declaration_count_this_file += wrapped_class.declaration_count;
        }
    }




    // if the last file set isn't empty, add that, too
    if (!classes_for_this_file.empty()) {
        classes_per_file.emplace_back(classes_for_this_file);
    }

    if (already_wrapped_classes.size() != WrappedClass::wrapped_classes.size()) {
        cerr << fmt::format("Could not wrap all classes - wrapped {} out of {}",
                            already_wrapped_classes.size(), WrappedClass::wrapped_classes.size()) << endl;
    }

    int total_file_count = classes_per_file.size();
    for (int i = 0; i < total_file_count; i++) {
        write_classes(i + 1, classes_per_file[i], i == total_file_count - 1);
    }


//    if (generate_v8classwrapper_sfinae) {
//        string sfinae_filename = fmt::format("v8toolkit_generated_v8classwrapper_sfinae.h", file_count);
//        ofstream sfinae_file;
//
//        sfinae_file.open(sfinae_filename, ios::out);
//        if (!sfinae_file) {
//            llvm::report_fatal_error(fmt::format( "Couldn't open {}", sfinae_filename).c_str());
//        }
//
//        sfinae_file << "#pragma once\n\n";
//
//        sfinae_file << get_sfinae_matching_wrapped_classes(WrappedClass::wrapped_classes) << std::endl;
//        sfinae_file.close();
//    }

    cerr << "Classes returned from matchers: " << matched_classes_returned << endl;

    cerr << "Classes used that were not wrapped" << endl;




    for ( auto & wrapped_class : WrappedClass::wrapped_classes) {
        if (!wrapped_class->dumped) { continue; }
        for (auto used_class : wrapped_class->used_classes) {
            if (!used_class->dumped) {
                cerr << fmt::format("{} uses unwrapped type: {}", wrapped_class->name_alias, used_class->name_alias) << endl;
            }
        }

    }

}

// takes a file number starting at 1 and incrementing 1 each time
// a list of WrappedClasses to print
// and whether or not this is the last file to be written
void PrintFunctionNamesAction::write_classes(int file_count, vector<WrappedClass*> & classes, bool last_one) {

    cerr << fmt::format("writing classes, file_count: {}, classes.size: {}, last_one: {}", file_count, classes.size(), last_one) << endl;
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

    for (WrappedClass * wrapped_class : classes) {

        for (auto derived_type : wrapped_class->derived_types) {
            extern_templates.insert(derived_type);
        }

        for (auto used_class : wrapped_class->used_classes) {
            if (used_class->should_be_wrapped()) {
                extern_templates.insert(used_class);
            }
        }


        if (print_logging) cerr << "Dumping " << wrapped_class->class_name << " to file " << class_wrapper_filename << endl;

        //                printf("While dumping classes to file, %s has includes: ", wrapped_class->class_name.c_str());

        // Combine the includes needed for types in members/methods with the includes for the wrapped class's
        //   derived types
        auto include_files = wrapped_class->include_files;
        auto base_type_includes = wrapped_class->get_derived_type_includes();
        include_files.insert(base_type_includes.begin(), base_type_includes.end());

        for(auto & include_file : include_files) {
            //  printf("%s ", include_file.c_str());
            if (include_file != "" && already_included_this_file.count(include_file) == 0) {

                // skip "internal looking" includes - look at 1 because 0 is < or "
                if (include_file.find("__") == 1) {
                    continue;
                }
                class_wrapper_file << fmt::format("#include {}\n", include_file);
                already_included_this_file.insert(include_file);
            }
        }
        //                printf("\n");
    }

    // remove any types that are wrapped in this file since it will be explicitly instantiated here
    for (auto wrapped_class : classes) {

        // DO NOT EXPLICITLY INSTANTIATE THE WRAPPED TYPE
//		if (wrapped_class->is_template_specialization()) {
//		    class_wrapper_file << "template " << wrapped_class->class_name << ";" << endl;
//		}
        class_wrapper_file << fmt::format("template class v8toolkit::V8ClassWrapper<{}>;\n", wrapped_class->class_name);
        // if it's not a template specialization it shouldn't be in the extern_template set, but delete it anyhow
        extern_templates.erase(wrapped_class);
    }

    for (auto extern_template : extern_templates) {
        if (extern_template->is_template_specialization()) {
            class_wrapper_file << "extern template " << extern_template->class_name << ";\n";
        }
        class_wrapper_file << fmt::format("extern template class v8toolkit::V8ClassWrapper<{}>;\n", extern_template->class_name);
    }



    // Write function header
    class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate &); // may not exist -- that's ok\n", file_count+1);


    if (file_count == 1) {
        class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers(v8toolkit::Isolate & isolate) {{\n");

    } else {
        class_wrapper_file << fmt::format("void v8toolkit_initialize_class_wrappers_{}(v8toolkit::Isolate & isolate) {{\n",
                                          file_count);
    }



    // Print function body
    for (auto wrapped_class : classes) {
        // each file is responsible for making explicit instantiatinos of its own types
        class_wrapper_file << wrapped_class->get_wrapper_string();
    }


    // if there's going to be another file, call the function in it
    if (!last_one) {
        class_wrapper_file << fmt::format("  v8toolkit_initialize_class_wrappers_{}(isolate);\n", file_count + 1);
    }

    // Close function and file
    class_wrapper_file << "}\n";
    class_wrapper_file.close();

}
