
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/library_extensions.h>
#include <xl/templates.h>
using xl::templates::ProviderPtr;


#include "../wrapped_class.h"

#include "../helper_functions.h"
#include "bindings_output.h"

using namespace xl::templates;

namespace v8toolkit::class_parser::bindings_output {

extern Template file_template;
extern Template class_template;
extern Template standard_includes_template;


extern std::map<string, Template> bindings_templates;

bool BindingsCriteria::class_filter(WrappedClass const & c) {
    cerr << "Checking class criteria" << endl;

    return true;
}


std::ostream & BindingsOutputStreamProvider::get_class_collection_stream() {
    this->count++;

    log.info(LogSubjects::BindingsOutput, "Starting bindings output file: {}", this->count);

    string class_wrapper_filename = fmt::format("v8toolkit_generated_class_wrapper_{}.cpp", this->count);

    this->output_stream.close();
    this->output_stream.open(class_wrapper_filename);

    return this->output_stream;
}


struct BindingsProviderContainer;

using P = xl::templates::DefaultProviders<BindingsProviderContainer>;


struct BindingsProviderContainer {



    static ProviderPtr get_provider(WrappedClass const & c) {
        auto provider = P::make_provider(
            std::pair("comment", c.comment),
            std::pair("name", c.get_short_name()),
            std::pair("alias", c.get_name_alias()),
            std::pair("data_members", std::ref(c.get_members())),
            std::pair("member_functions", std::ref(c.get_member_functions())),
            std::pair("static_functions", std::ref(c.get_static_functions())),
            std::pair("constructor", c.get_constructors().size() == 0 || c.force_no_constructors ?
                                     fmt::format("class_wrapper.expose_static_methods(\"{}\", isolate);",
                                                 c.get_name_alias()) :
                                     fmt::format(
                                         "class_wrapper.add_constructor<{}>(\"{}\", isolate, {});",
                                         c.get_constructors().back()->get_return_and_class_and_parameter_types_string(),
                                         c.get_name_alias(), c.get_constructors().back()->get_default_argument_tuple_string())),

            std::pair("inheritance",
                      fmt::format("{}", c.base_types.empty() ? "" : (*c.base_types.begin())->get_name_alias())),
            std::pair("base_type_name", c.base_types.empty() ? "" : (*c.base_types.begin())->get_short_name()),
            xl::forward_as_pair("derived_types", c.derived_types)
        );

        return provider;
    }


    static ProviderPtr get_provider(DataMember const & d) {
        return P::make_provider(
            std::pair("comment", d.comment),
            std::pair("name", d.js_name),
            std::pair("declared_in", d.declared_in.class_name),
            std::pair("type", d.type.get_name())
        );

    }


    static ProviderPtr get_provider(MemberFunction const & f) {
        return P::make_provider(
            std::pair("name", f.js_name),
            std::pair("comment", f.comment),
            std::pair("binding_parameters", f.get_return_and_class_and_parameter_types_string()),
            std::pair("parameters", P::make_provider(f.parameters)),
            std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
            std::pair("return_comment", f.return_type_comment),
            std::pair("class_name", f.wrapped_class.get_short_name())
        );
    }


    static ProviderPtr get_provider(StaticFunction const & f) {
        return P::make_provider(
            std::pair("name", f.js_name),
            std::pair("comment", f.comment),
            std::pair("binding_parameters", f.get_return_and_class_and_parameter_types_string()),
            std::pair("parameters", P::make_provider(f.parameters)),
            std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
            std::pair("return_comment", f.return_type_comment),
            std::pair("class_name", f.wrapped_class.get_short_name())
        );
    }


    static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
        return P::make_provider(
            std::pair("type", p.type.get_jsdoc_type_name()),
            std::pair("name", p.name),
            std::pair("comment", p.description)
        );
    }


    static ProviderPtr get_provider(ClassFunction::TypeInfo const & t) {
        return P::make_provider("Implement me");
    }

}; // end BindingsProviderContainer



struct BindingFile {
    size_t max_declaration_count;
    size_t declaration_count = 0;

    BindingFile(size_t max_declaration_count) :
        max_declaration_count(max_declaration_count)
    {}

    /**
     * Whether this binding file can hold the specified WrappedClass
     * @param wrapped_class WrappedClass to see if this BindingFile can hold it
     * @return true if it can be held without exceeding declaration limit, false otherwise
     */
    bool can_hold(WrappedClass const * wrapped_class) {
        return declaration_count == 0 ||
               max_declaration_count == 0 ||
               this->declaration_count + wrapped_class->declaration_count <= max_declaration_count;
    }

    std::vector<WrappedClass const *> classes;

    // this is currently unused - it's not the clear win that it used to be
    std::set<WrappedClass const *> extern_templates;

    std::set<WrappedClass const *> explicit_instantiations;

    std::set<std::string> includes;

    std::vector<WrappedClass const *> const & get_classes() const {return this->classes;}
    auto & get_includes() const {return includes;}
    auto & get_explicit_instantiations()        const { return this->explicit_instantiations; }
    auto & get_extern_template_instantiations() const { return this->extern_templates; }

    void add_class(WrappedClass const * wrapped_class) {
//        std::cerr << fmt::format("adding to BindingFile: {}", wrapped_class->get_name_alias()) << std::endl;
        this->classes.push_back(wrapped_class);
        this->declaration_count += wrapped_class->declaration_count;
        assert(this->declaration_count <= this->max_declaration_count);


        auto base_type_includes = wrapped_class->get_base_type_includes();
//        std::cerr << fmt::format("adding base type includes: {}", join(base_type_includes)) << std::endl;
        includes.insert(base_type_includes.begin(), base_type_includes.end());

        auto derived_type_includes = wrapped_class->get_derived_type_includes();
//        std::cerr << fmt::format("adding derived type includes: {}", join(derived_type_includes)) << std::endl;
        includes.insert(derived_type_includes.begin(), derived_type_includes.end());

        this->explicit_instantiations.insert(wrapped_class);

    }

};


void BindingsOutputModule::process(std::vector < WrappedClass const*> const & wrapped_classes)
{
//    std::cerr << fmt::format("file template contents: {}", file_template.c_str()) << std::endl;

//    std::cerr << fmt::format("making bindings output") << std::endl;
//    std::cerr << fmt::format("all binding classes:") << std::endl;
//    for (auto c : wrapped_classes) {
//        std::cerr << fmt::format("{}: {}, derived_types: {}", c->get_short_name(), (void*)c, (void*)&c->derived_types) << std::endl;
//        for(auto d : c->derived_types) {
//            std::cerr << fmt::format(" - derived: {}", (void*)d) << std::endl;
//        }
//    }
    std::vector<BindingFile> binding_files{BindingFile(this->max_declarations_per_file)};
    BindingsOutputStreamProvider stream_provider;

    std::set<WrappedClass const *> already_wrapped_classes;


    // go through all the classes until a full pass has been made with nothing new to be written out
    bool found_match = true;
    while (found_match) {
        found_match = false;

        // go through the list to see if there is anything left to write out
        for (auto & wrapped_class : wrapped_classes) {

            // if it has unmet dependencies or has already been mapped, skip it
            if (!wrapped_class->ready_for_wrapping(already_wrapped_classes)) {
//                std::cerr << fmt::format("Skipping because not ready_for_wrapping: {}", wrapped_class->class_name) << std::endl;
                continue;
            }
            already_wrapped_classes.insert(wrapped_class);
            found_match = true;

//            std::cerr << fmt::format("writing class {} to file with declaration_count = {}", wrapped_class->get_name_alias(),
//                                     wrapped_class->declaration_count) << std::endl;


            if (!binding_files.back().can_hold(wrapped_class)) {

//                std::cerr << fmt::format("Out of space in file, rotating") << std::endl;
                binding_files.emplace_back(this->max_declarations_per_file);
            }

            binding_files.back().add_class(wrapped_class);

            // assert false - this shouldn't alter it
            already_wrapped_classes.insert(wrapped_class);
        }
    }


    if (already_wrapped_classes.size() != WrappedClass::wrapped_classes.size()) {
//        cerr << fmt::format("Could not wrap all classes - wrapped {} out of {}",
//                            already_wrapped_classes.size(), WrappedClass::wrapped_classes.size()) << endl;
    }

//    std::cerr << fmt::format("about to dump {} binding_files", binding_files.size()) << std::endl;
    for (int i = 0; i < binding_files.size(); i++) {

        auto & binding_file = binding_files[i];
        bool last_file = i == binding_files.size() - 1;
        int file_number = i + 1;

        // this takes care of providing the correct stream for each subsequent call
        auto & output_stream = stream_provider.get_class_collection_stream();
//        std::cerr << fmt::format("bindings_templates[file] contents: {}", bindings_templates["file"].c_str()) << std::endl;
        auto template_result = bindings_templates["file"].fill<BindingsProviderContainer>(
            P::make_provider(
                std::pair("file_number", fmt::format("{}", file_number)),
                std::pair("next_file_number", fmt::format("{}", file_number + 1)), // ok if it won't actually exist
                std::pair("classes", P::make_provider(binding_file.get_classes())),
                std::pair("includes", P::make_provider(std::bind(&BindingFile::get_includes, binding_file))),
                std::pair("extern_templates", P::make_provider(std::bind(&BindingFile::get_extern_template_instantiations, binding_file))),
                std::pair("explicit_instantiations", binding_file.get_explicit_instantiations()),
                std::pair("call_next_function", !last_file ? fmt::format("v8toolkit_initialize_class_wrappers_{}(isolate);", file_number + 1) : "")
            ),
            bindings_templates
        );

        log.info(LogSubjects::Subjects::BindingsOutput, "Writing binding file {}", file_number);
        log.info(LogSubjects::Subjects::BindingsOutput, template_result);
        output_stream << template_result << std::flush;
    }




//    cerr << "Classes used that were not wrapped:" << endl;
    for (auto & wrapped_class : wrapped_classes) {
        if (!xl::contains(already_wrapped_classes, wrapped_class)) {
            for (auto used_class : wrapped_class->used_classes) {
                if (already_wrapped_classes.count(used_class) == 0) {
                    log.error(LogSubjects::Class, "Could not dump '{}' because it uses type '{}' that wasn't dumped", wrapped_class->get_name_alias(),
                                        used_class->get_name_alias());
                }
            }
        }
    }
}

string BindingsOutputModule::get_name() {
    return "BindingsOutputModule";
}


Template class_template(R"({
    v8toolkit::V8ClassWrapper<{{name}}> & class_wrapper = isolate.wrap_class<{{name}}>();
    class_wrapper.set_class_name("{{alias}}");
{{<member_functions|!!
    class_wrapper.add_method<{{binding_parameters}}>("{{name}}", &{{class_name}}::{{name}}, std::tuple<>());>}}
{{<static_functions|!!
    class_wrapper.add_static_method<{{binding_parameters}}>("{{name}}", &{{class_name}}::{{name}}, std::tuple<>());>}}
{{<data_members|!!
    class_wrapper.add_member<{{type}}, {{declared_in}}>("{{name}}");>}}

    class_wrapper.set_parent_type<{{<base_type_name>}}>();


    class_wrapper.set_compatible_types<{{<derived_types|!name>}}>();

    class_wrapper.finalize(true);
    {{<constructor>}}
})");


Template file_template(R"(
#### FILE: {{file_number}} ####

{{!standard_includes}}
// includes
{{<includes|!!
#include {{<include>}}>}}
// /includes

// explicit instantiations
{{<explicit_instantiations|!!
template class {{<name>}}>}}
// /explicit instantiations

{{<extern_templates|!!
extern template {{class_name}}>}}

void v8toolkit_initialize_class_wrappers_{{next_file_number}}(v8toolkit::Isolate &); // may not exist -- that's ok
void v8toolkit_initialize_class_wrappers(v8toolkit::Isolate & isolate) {
{


{{classes|class}}


{{call_next_function}}

}
)");


Template standard_includes_template(R"(
#include "js_casts.h"
#include <v8toolkit/v8_class_wrapper_impl.h>

)");


std::map<string, Template> bindings_templates {
    std::pair("class", class_template),
    std::pair("file", file_template),
    std::pair("standard_includes", standard_includes_template)
};


} // end namespace v8toolkit::class_parser::bindings_output