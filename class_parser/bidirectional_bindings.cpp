#if 0
void BidirectionalBindings::generate_bindings() {
    std::stringstream result;
    auto matches = wrapped_class.annotations.get_regex("v8toolkit_generate_(.*)");
    if (wrapped_class.annotations.has(V8TOOLKIT_BIDIRECTIONAL_CLASS_STRING)) {
        result << fmt::format("class JS{} : public {}, public v8toolkit::JSWrapper<{}> {{\npublic:\n", // {{ is escaped {
                              short_name(), short_name(), short_name());
        result << fmt::format("    JS{}(v8::Local<v8::Context> context, v8::Local<v8::Object> object,\n", short_name());
        result << fmt::format("        v8::Local<v8::FunctionTemplate> created_by");
        bool got_constructor = false;
        int constructor_parameter_count;
        vector<QualType> constructor_parameters;
        foreach_constructor(wrapped_class.decl, [&](auto constructor_decl){
            if (got_constructor) {
                data_error(fmt::format("ERROR: Got more than one constructor for {}", wrapped_class.class_name));
                return;
            }
            got_constructor = true;
            result << get_method_parameters(compiler_instance, wrapped_class, constructor_decl, true, true);
            constructor_parameter_count = constructor_decl->getNumParams();
            constructor_parameters = get_method_param_qual_types(this->compiler_instance, constructor_decl);

        }, V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR_STRING);
        if (!got_constructor) {
            data_error(fmt::format("ERROR: Got no bidirectional constructor for {}", wrapped_class.class_name));

        }
        result << fmt::format(") :\n");

        //                auto variable_names = generate_variable_names(constructor_parameter_count);
        auto variable_names = generate_variable_names(constructor_parameters, true);

        result << fmt::format("      {}({}),\n", short_name(), join(variable_names));
        result << fmt::format("      v8toolkit::JSWrapper<{}>(context, object, created_by) {{}}\n", short_name()); // {{}} is escaped {}
        result << handle_class(wrapped_class.decl);
        result << "};\n";
    } else {
//                printf("Class %s not marked bidirectional\n", short_name().c_str());
        return;
    }

    // dumps a file per class
//            if (print_logging) cerr << "Dumping JSWrapper type for " << short_name() << endl;
    ofstream bidirectional_class_file;
    auto bidirectional_class_filename = fmt::format("v8toolkit_generated_bidirectional_{}.h", short_name());
    bidirectional_class_file.open(bidirectional_class_filename, ios::out);
    if (!bidirectional_class_file) {
        llvm::report_fatal_error(fmt::format("Could not open file: {}", bidirectional_class_filename), false);
    }

    bidirectional_class_file << "#pragma once\n\n";


    // This needs include files because the IMPLEMENTATION goes in the file (via macros).
    // If the implementation was moved out to a .cpp file, then the header file could
    //   rely soley on the primary type's includes
    for (auto & include : this->wrapped_class.include_files) {
        if (include == ""){continue;}
        bidirectional_class_file << "#include " << include << "\n";
    }


    // need to include all the includes from the parent types because the implementation of this bidirectional
    //   type may need the types for things the parent type .h files don't need (like unique_ptr contained types)
    auto all_base_type_includes = this->wrapped_class.get_base_type_includes();

    for (auto & include : all_base_type_includes) {
        std::cerr << fmt::format("for bidirectional {}, adding base type include {}", this->short_name(), include) << std::endl;
        bidirectional_class_file << "#include " << include << "\n";
    }

    bidirectional_class_file << result.str();
    bidirectional_class_file.close();


}

#endif