
#include <iostream>
#include <fmt/ostream.h>
#include <vector>
#include <sstream>

#include <xl/templates.h>
#include <xl/library_extensions.h>
using xl::templates::ProviderPtr;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/AST/DeclCXX.h"
#pragma clang diagnostic pop


#include "../wrapped_class.h"
#include "../parsed_method.h"
#include "javascript_subclass_template.h"
#include "../helper_functions.h"

//MAY BE OTHER ISSUES WITH CONSTRUCTOR PARAMETERS


namespace v8toolkit::class_parser::javascript_subclass_template_output {

using namespace xl::templates;

extern xl::templates::Template class_template;
extern std::map<string, xl::templates::Template> bidirectional_templates;


bool JavascriptSubclassTemplateCriteria::operator()(WrappedClass const & c) {
    if (!c.bidirectional) {
        log.info(LogSubjects::Subjects::BidirectionalOutput,
                 "BidirectionalCriteria: skipping {} because not bidirectional", c.class_name);
        return false;
    }

    if (c.base_types.size() != 1) {
        log.error(LogSubjects::Subjects::BidirectionalOutput,
                  "BidirectionalCriteria: bidirectional class {} must have 1 base type but actually has {} - {}",
                  c.get_js_name(), c.base_types.size(), xl::join(c.base_types));
        return false;
    }

    return true;
}


std::ostream & JavascriptSubclassTemplateOutputStreamProvider::get_class_collection_stream() {
    return std::cerr;
}

ostream & JavascriptSubclassTemplateOutputStreamProvider::get_class_stream(WrappedClass const & c) {
    this->output_file.close();
    auto filename = fmt::format("_{}.js", c.class_name);
    this->output_file.open(filename);
    if (!this->output_file) {

    }
    return this->output_file;
}





struct JavascriptSubclassTemplateProviderContainer {

    static ProviderPtr get_provider(WrappedClass const & c) {

        std::vector<ClassFunction const *> virtual_functions;
        c.foreach_inheritance_level([&](auto & c) {
            for(auto & f : c.get_member_functions()) {
//                std::cerr << fmt::format("{} virtual: {} static: {}", f->name, f->is_virtual, f->is_static) << std::endl;
                if (f->is_virtual && !f->is_virtual_override && !f->is_virtual_final) {
                    virtual_functions.push_back(f.get());
                }
            }
        });

        // static functions shouldn't be included
//        c.foreach_inheritance_level([&](auto & c) {
//            for(auto & f : c.get_static_functions()) {
////                std::cerr << fmt::format("{} virtual: {} static: {}", f->name, f->is_virtual, f->is_static) << std::endl;
//                virtual_functions.push_back(f.get());
//            }
//        });

        return xl::templates::make_provider<JavascriptSubclassTemplateProviderContainer>(
            std::pair("virtual_functions", virtual_functions),
            std::pair("data_members", (*c.base_types.begin())->get_members()),
            std::pair("data_members_2", (*c.base_types.begin())->get_members())
        );
    }


    static ProviderPtr get_provider(ClassFunction const & f) {
        return xl::templates::make_provider<JavascriptSubclassTemplateProviderContainer>(
            std::pair("js_name", f.js_name),
            std::pair("js_return_type", f.return_type.get_jsdoc_type_name())
        );
    }


    static ProviderPtr get_provider(DataMember const & d) {

        return xl::templates::make_provider<JavascriptSubclassTemplateProviderContainer>(
            std::pair("js_name", d.js_name),
            std::pair("js_type", d.type.get_jsdoc_type_name())
        );
    }

    static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
        return xl::templates::make_provider<JavascriptSubclassTemplateProviderContainer>("");
    }

    static ProviderPtr get_provider(TypeInfo const & t) {
        return xl::templates::make_provider<JavascriptSubclassTemplateProviderContainer>("");
    }
};



void JavascriptSubclassTemplateOutputModule::process(std::vector < WrappedClass const*> wrapped_classes)
{

    JavascriptSubclassTemplateOutputStreamProvider stream_provider;

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Starting JavascriptSubclassTemplateOutputModule output module");
    log.info(LogT::Subjects::BidirectionalOutput, "Bidirectional wrapped classes count: {}",
             wrapped_classes.size());

    for(auto c : wrapped_classes) {
        log.info(LogT::Subjects::BidirectionalOutput, "Creating javascript subclass template output for class: {}",
                 c->class_name);
        auto & ostream = this->output_stream_provider->get_class_stream(*c);

        ostream << *bidirectional_templates["class"].template fill<JavascriptSubclassTemplateProviderContainer>(std::ref(*c), bidirectional_templates);
    }

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Finished javascript subclass template output module");

}

string JavascriptSubclassTemplateOutputModule::get_name() {
    return "BidirectionalOutputModule";
}

OutputCriteria & JavascriptSubclassTemplateOutputModule::get_criteria() {
    return this->criteria;
}





::xl::templates::Template javascript_subtype_template_template(R"(

exports.create = function(exports, world_creation, base_type) {

        return base_type.subclass(
            // JavaScript prototype object
            {
{{<<virtual_functions|!!
                /**
                 * @return \{{{js_return_type}}\}
                 */
                // {{js_name}}: ()=>{...IMPLEMENT ME...},>>}}
            },

            // Per-object initialization
           /**
{{<<data_members_2|!!
            * @property \{{{js_type}}\} {{js_name}}}}
            */
            function() {
{{<<data_members|!!
                // this.{{js_name}} = ...VALUE...;>>}}
            }
        ); // end base_type.subclass
} // end create function
)");


std::map<string, Template> bidirectional_templates {
    std::pair("class", javascript_subtype_template_template),
};



} // end namespace v8toolkit::class_parser::bidirectional_output