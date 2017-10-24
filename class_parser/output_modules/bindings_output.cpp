
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/library_extensions.h>
#include <xl/templates.h>
using xl::templates::ProviderPtr;


#include "../wrapped_class.h"

#include "../helper_functions.h"
#include "bindings_output.h"

namespace v8toolkit::class_parser {

bool BindingsCriteria::class_filter(WrappedClass const & c) {
    cerr << "Checking class criteria" << endl;

//        if (c.get_name_alias().find("<") != std::string::npos) {
//            std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
//                                     c.get_name_alias()) << std::endl;
//            return false;
//        } else if (c.base_types.size() > 0 && (*c.base_types.begin())->get_name_alias().find("<") != std::string::npos) {
//            std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
//                                     c.get_name_alias(),
//                                     (*c.base_types.begin())->get_name_alias()) << std::endl;
//            return false;
//        } else if (c.bidirectional) {
//            std::cerr << fmt::format("Skipping generation of js stub for {} because it's a bidirectional type", c.get_name_alias()) << std::endl;
//            return false;
//        }

    return true;
}


std::ostream & BindingsOutputStreamProvider::get_class_collection_stream() {
    this->count++;

    log.info(LogSubjects::BindingsOutput, "Starting bindings output file: {}", this->count);

//    string class_wrapper_filename = fmt::format("v8toolkit_generated_class_wrapper_{}.cpp", this->count);
//
//    this->output_stream.close();
//    this->output_stream.open(class_wrapper_filename);
//
//    return this->output_stream;

    return std::cerr;
}



static ProviderPtr get_provider(WrappedClass const * c) {
    return xl::templates::make_provider(
        std::pair("comment", c->comment),
        std::pair("name", c->get_name_alias()),
        std::pair("data_members", xl::templates::make_provider(c->get_members())),
        std::pair("constructors", xl::templates::make_provider(c->get_constructors())),
        std::pair("member_functions", xl::templates::make_provider(c->get_member_functions())),
        std::pair("static_functions", xl::templates::make_provider(c->get_static_functions())),
        std::pair("inheritance", fmt::format("{}", c->base_types.empty() ? "" : (*c->base_types.begin())->get_name_alias()))
    );
}


static ProviderPtr get_provider(DataMember const & d) {
    return xl::templates::make_provider(
        std::pair("comment", d.comment),
        std::pair("name", d.js_name),
        std::pair("type", d.type.get_jsdoc_type_name())
    );
}


static ProviderPtr get_provider(ConstructorFunction const & f) {
    return xl::templates::make_provider(
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters))
    );
}


static ProviderPtr get_provider(MemberFunction const & f) {
    return xl::templates::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters)),
        std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
        std::pair("return_comment", f.return_type_comment)
    );
}


static ProviderPtr get_provider(StaticFunction const & f) {
    return xl::templates::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters)),
        std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
        std::pair("return_comment", f.return_type_comment)
    );
}



static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
    return xl::templates::make_provider(
        std::pair("type", p.type.get_jsdoc_type_name()),
        std::pair("name", p.name),
        std::pair("comment", p.description)
    );
}


static ProviderPtr get_provider(ClassFunction::TypeInfo const & t) {
    return xl::templates::make_provider("Implement me");

}



void BindingsOutputModule::process(std::vector < WrappedClass const*> const & wrapped_classes)
{
    BindingsOutputStreamProvider stream_provider;

    auto bindings_templates = xl::templates::load_templates("bindings_templates");

    // current file number for bindings file so it can be broken down into multiple files
    int file_count = 1;

    // start at one so they are never considered "empty"
    int declaration_count_this_file = 1;
    vector<WrappedClass const *> classes_for_this_file;
    set<WrappedClass const *> already_wrapped_classes;
    vector<vector<WrappedClass const *>> classes_per_file;
    std::set<WrappedClass const *> dumped;



    // go through all the classes until a full pass has been made with nothing new to be written out
    bool found_match = true;
    while (found_match) {
        found_match = false;

        // go through the list to see if there is anything left to write out
        for (auto & wrapped_class : wrapped_classes) {

            // if it has unmet dependencies or has already been mapped, skip it
            if (!wrapped_class->ready_for_wrapping(already_wrapped_classes)) {
                std::cerr << fmt::format("Skipping because not ready_for_wrapping: {}", wrapped_class->class_name) << std::endl;
                continue;
            }
            already_wrapped_classes.insert(wrapped_class);
            found_match = true;

            std::cerr << fmt::format("writing class {} to file with declaration_count = {}", wrapped_class->get_name_alias(),
                                     wrapped_class->declaration_count) << std::endl;

            // if there's room in the current file, add this class
            auto space_available = declaration_count_this_file == 0 || this->max_declarations_per_file == 0 ||
                                   declaration_count_this_file + wrapped_class->declaration_count <
                                   this->max_declarations_per_file;

            if (!space_available) {


                std::cerr << fmt::format("Out of space in file, rotating") << std::endl;
                classes_per_file.emplace_back(classes_for_this_file);

                // reset for next file
                classes_for_this_file.clear();
                declaration_count_this_file = 0;
                file_count++;
            }

            classes_for_this_file.push_back(wrapped_class);

            // assert false - this shouldn't alter it
            dumped.insert(wrapped_class);
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
    for (auto & classes : classes_per_file)
    for (int i = 0; i < total_file_count; i++) {

        // this takes care of providing the correct stream for each subsequent call
        auto & output_stream = stream_provider.get_class_collection_stream();
        auto template_result = bindings_templates["file"].fill(
            xl::templates::make_provider(std::pair("classes", xl::templates::make_provider(classes))),
            bindings_templates
        );

        output_stream << template_result;
    }


    cerr << "Classes returned from matchers: " << matched_classes_returned << endl;


    cerr << "Classes used that were not wrapped:" << endl;
    for (auto & wrapped_class : wrapped_classes) {
        if (!xl::contains(dumped, wrapped_class)) {
            for (auto used_class : wrapped_class->used_classes) {
                if (dumped.count(used_class) == 0) {
                    log.error(LogSubjects::Class, "Could not dump '{}' because it uses type '{}' that wasn't dumped", wrapped_class->get_name_alias(),
                                        used_class->get_name_alias());
                }
            }
        }
    }
}


} // end namespace v8toolkit::class_parser