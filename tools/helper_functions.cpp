

#include "helper_functions.h"

#include <iostream>
#include <fstream>
#include <fmt/ostream.h>

#include "wrapped_class.h"


// Gets the "most basic" type in a type.   Strips off ref, pointer, CV
//   then calls out to get how to include that most basic type definition
//   and puts it in wrapped_class.include_files
void update_wrapped_class_for_type(CompilerInstance & compiler_instance,
                                   WrappedClass & wrapped_class,
    // don't capture qualtype by ref since it is changed in this function
                                   QualType qual_type) {

    cerr << fmt::format("In update_wrapped_class_for_type {} in wrapped class {}", qual_type.getAsString(), wrapped_class.class_name) << endl;

    if (print_logging) cerr << "Went from " << qual_type.getAsString();
    qual_type = qual_type.getLocalUnqualifiedType();


    // remove pointers
    while(!qual_type->getPointeeType().isNull()) {
        qual_type = qual_type->getPointeeType();
    }
    qual_type = qual_type.getLocalUnqualifiedType();

    if (print_logging) cerr << " to " << qual_type.getAsString() << endl;
    auto base_type_record_decl = qual_type->getAsCXXRecordDecl();



    if (auto function_type = dyn_cast<FunctionType>(&*qual_type)) {
        cerr << "IS A FUNCTION TYPE!!!!" << endl;

        // it feels strange, but the type int(bool) from std::function<int(bool)> is a FunctionProtoType
        if (auto function_prototype = dyn_cast<FunctionProtoType>(function_type)) {
            cerr << "IS A FUNCTION PROTOTYPE" << endl;

            cerr << "Recursing on return type" << endl;
            update_wrapped_class_for_type(compiler_instance, wrapped_class, function_prototype->getReturnType());

            for ( auto param : function_prototype->param_types()) {

                cerr << "Recursing on param type" << endl;
                update_wrapped_class_for_type(compiler_instance, wrapped_class, param);
            }
        } else {
            cerr << "IS NOT A FUNCTION PROTOTYPE" << endl;
        }

    } else {
        cerr << "is not a FUNCTION TYPE" << endl;
    }


    // primitive types don't have record decls
    if (base_type_record_decl == nullptr) {
        return;
    }

    auto actual_include_string = get_include_for_type_decl(compiler_instance, base_type_record_decl);

    if (print_logging) cerr << &wrapped_class << "Got include string for " << qual_type.getAsString() << ": " << actual_include_string << endl;

    // if there's no wrapped type, it may be something like a std::function or STL container -- those are ok to not be wrapped
    if (has_wrapped_class(base_type_record_decl)) {
        auto & used_wrapped_class = WrappedClass::get_or_insert_wrapped_class(base_type_record_decl, compiler_instance, FOUND_UNSPECIFIED);
        wrapped_class.used_classes.insert(&used_wrapped_class);
    }



    wrapped_class.include_files.insert(actual_include_string);



    if (dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl)) {
        if (print_logging) cerr << "##!#!#!#!# Oh shit, it's a template type " << qual_type.getAsString() << endl;

        auto template_specialization_decl = dyn_cast<ClassTemplateSpecializationDecl>(base_type_record_decl);

        // go through the template args
        auto & template_arg_list = template_specialization_decl->getTemplateArgs();
        for (decltype(template_arg_list.size()) i = 0; i < template_arg_list.size(); i++) {
            auto & arg = template_arg_list[i];

            // this code only cares about types, so skip non-type template arguments
            if (arg.getKind() != clang::TemplateArgument::Type) {
                continue;
            }
            auto template_arg_qual_type = arg.getAsType();
            if (template_arg_qual_type.isNull()) {
                if (print_logging) cerr << "qual type is null" << endl;
                continue;
            }
            if (print_logging) cerr << "Recursing on templated type " << template_arg_qual_type.getAsString() << endl;
            update_wrapped_class_for_type(compiler_instance, wrapped_class, template_arg_qual_type);
        }
    } else {
        if (print_logging) cerr << "Not a template specializaiton type " << qual_type.getAsString() << endl;
    }
}





EXPORT_TYPE get_export_type(const NamedDecl * decl, EXPORT_TYPE previous) {
    auto &attrs = decl->getAttrs();
    EXPORT_TYPE export_type = previous;

    auto name = decl->getNameAsString();

    bool found_export_specifier = false;

    for (auto attr : attrs) {
        if (dyn_cast<AnnotateAttr>(attr)) {
            auto attribute_attr = dyn_cast<AnnotateAttr>(attr);
            auto annotation_string = attribute_attr->getAnnotation().str();

            if (annotation_string == V8TOOLKIT_ALL_STRING) {
                if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name));}

                export_type = EXPORT_ALL;
                found_export_specifier = true;
            } else if (annotation_string == "v8toolkit_generate_bindings_some") {
                if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name));}
                export_type = EXPORT_SOME;
                found_export_specifier = true;
            } else if (annotation_string == "v8toolkit_generate_bindings_except") {
                if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                export_type = EXPORT_EXCEPT;
                found_export_specifier = true;
            } else if (annotation_string == V8TOOLKIT_NONE_STRING) {
                if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                export_type = EXPORT_NONE; // just for completeness
                found_export_specifier = true;
            }
        }
    }

    // go through bases looking for specific ones
    if (const CXXRecordDecl * record_decl = dyn_cast<CXXRecordDecl>(decl)) {
        for (auto & base : record_decl->bases()) {
            auto type = base.getType();
            auto base_decl = type->getAsCXXRecordDecl();
            auto base_name = get_canonical_name_for_decl(base_decl);
//                cerr << "%^%^%^%^%^%^%^% " << get_canonical_name_for_decl(base_decl) << endl;
            if (base_name == "class v8toolkit::WrappedClassBase") {
                cerr << "FOUND WRAPPED CLASS BASE -- EXPORT_ALL" << endl;
                if (found_export_specifier) { data_error(fmt::format("Found more than one export specifier on {}", name).c_str());}
                export_type = EXPORT_ALL;
                found_export_specifier = true;
            }
        }
    }

    //        printf("Returning export type: %d for %s\n", export_type, name.c_str());
    return export_type;
}



std::string get_canonical_name_for_decl(const TypeDecl * decl) {
    if (decl == nullptr) {
        llvm::report_fatal_error("null type decl to get_canonical_name_for_decl");
    }
    return decl->getTypeForDecl()->getCanonicalTypeInternal().getAsString();
}

//
//    std::string get_full_name_for_type(QualType qual_type) {
//
//        if (auto typedef_decl = qual_type->getAs<TypedefNameDecl>()) {
//            qual_type = typedef_decl->getUnderlyingType();
//        }
//        auto record_decl = qual_type->getAsCXXRecordDecl();
//        if (record_decl == nullptr) {
//            std::cerr << fmt::format("Couldn't get record decl for {}", qual_type.getAsString()) << std::endl;
//            assert(record_decl != nullptr);
//        }
//
//        return get_canonical_name_for_decl(record_decl);
//    }




// list of data (source code being processed) errors occuring during the run.   Clang API errors are still immediate fails with
//   llvm::report_fatal_error
vector<string> data_errors;
void data_error(const string & error) {
    cerr << "DATA ERROR: " << error << endl;
    data_errors.push_back(error);
}

vector<string> data_warnings;
void data_warning(const string & warning) {
    cerr << "DATA WARNING: " << warning << endl;
    data_warnings.push_back(warning);
}

QualType get_plain_type(QualType qual_type) {
    auto type = qual_type.getNonReferenceType().getUnqualifiedType();
    while(!type->getPointeeType().isNull()) {
        type = type->getPointeeType().getUnqualifiedType();
    }
    return type;
}



void generate_bindings() {

    // current file number for bindings file so it can be broken down into multiple files
    int file_count = 1;

    // start at one so they are never considered "empty"
    int declaration_count_this_file = 1;

    vector<WrappedClass*> classes_for_this_file;

    set<WrappedClass *> already_wrapped_classes;

    vector<vector<WrappedClass *>> classes_per_file;

    ofstream js_stub;


    cerr << fmt::format("About to start writing out wrapped classes with {} potential classes", WrappedClass::wrapped_classes.size()) << endl;


    // go through all the classes until a full pass has been made with nothing new to be written out
    bool found_match = true;
    while (found_match) {
        found_match = false;

        // go through the list to see if there is anything left to write out
        for (auto wrapped_class : WrappedClass::wrapped_classes) {

            cerr << fmt::format("considering dumping class: {}", wrapped_class->class_name) << endl;

            // if it has unmet dependencies or has already been mapped, skip it
            if (!wrapped_class->ready_for_wrapping(already_wrapped_classes)) {
                std::cerr << fmt::format("Skipping {}", wrapped_class->class_name) << std::endl;
                continue;
            }
            already_wrapped_classes.insert(wrapped_class);
            found_match = true;

            std::cerr << fmt::format("writing class {} to file with declaration_count = {}", wrapped_class->name_alias, wrapped_class->declaration_count) << std::endl;

            // if there's room in the current file, add this class
            auto space_available = declaration_count_this_file == 0 ||
                                   declaration_count_this_file + wrapped_class->declaration_count <
            MAX_DECLARATIONS_PER_FILE;

            if (!space_available) {


                std::cerr << fmt::format("Out of space in file, rotating") << std::endl;
                classes_per_file.emplace_back(classes_for_this_file);

                // reset for next file
                classes_for_this_file.clear();
                declaration_count_this_file = 0;
                file_count++;
            }

            classes_for_this_file.push_back(wrapped_class);
            wrapped_class->dumped = true;
            declaration_count_this_file += wrapped_class->declaration_count;
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

void generate_javascript_stub(std::string const & filename) {
    ofstream js_stub;
    // This filename shouldn't be hard coded - especially not to this value
    js_stub.open(filename, ios::out);
    if (!js_stub) {
        throw std::exception();
    }

    // write hard-coded text to top of apb api file
    js_stub << js_api_header << std::endl;

    for (auto * wrapped_class : WrappedClass::wrapped_classes) {
        if (wrapped_class->should_be_wrapped()) {
            js_stub << wrapped_class->generate_js_stub();
        }
    }
    js_stub.close();



}

void generate_bidirectional_classes() {

};

// converts from c++ type to javascript type
string convert_type_to_jsdoc(std::string const & type_name_input) {
    string type_name = type_name_input;
    std::smatch matches;
    std::cerr << fmt::format("converting {}...", type_name) << std::endl;

    // remove any leading struct/class text
    type_name = regex_replace(type_name, std::regex("^(struct|class) "), "");

    // things like: change vector<int> to Array.[Number]
    for (auto &pair : cpp_to_js_type_conversions) {

        if (regex_match(type_name, matches, std::regex(pair.first))) {
            std::cerr << fmt::format("matched {}, converting to {}", pair.first, pair.second) << std::endl;

            string replacement_type = pair.second; // need a temp because the regex matches point into the current this->type

            // go through each capturing match and...
            for (size_t i = 1; i < matches.size(); i++) {
//                // recursively convert the matched type
                string converted_captured_type_name = convert_type_to_jsdoc(matches[i].str());

                // look for $1, $2, etc in replacement and substitute in the matching position
                replacement_type = std::regex_replace(replacement_type, std::regex(fmt::format("\\${}", i)),
                                              converted_captured_type_name);
            }
            type_name = replacement_type;
            std::cerr << fmt::format("... final conversion to: {}", replacement_type) << std::endl;
            break;
        }
    }

    std::cerr << fmt::format("returning jsdoc converted type: {}", type_name) << std::endl;
    return type_name;
}








#if 0
//ifdef TEMPLATE_INFO_ONLY
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