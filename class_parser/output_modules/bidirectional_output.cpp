
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/templates.h>
#include <xl/templates/directory_loader.h>
#include <xl/library_extensions.h>
using xl::templates::ProviderPtr;


#include "../wrapped_class.h"
#include "bidirectional_output.h"

namespace v8toolkit::class_parser {

bool BidirectionalCriteria::operator()(WrappedClass const & c) {
    if (!c.bidirectional) {
        log.info(LogSubjects::Subjects::BidirectionalOutput, "BidirectionalCriteria: skipping {} because not bidirectional", c.get_name_alias());
        return false;
    }

    if (c.base_types.size() != 1) {
        log.error(LogSubjects::Subjects::BidirectionalOutput,
                  "BidirectionalCriteria: bidirectional class {} must have 1 base type but actually has {} - {}",
                  c.get_name_alias(), c.base_types.size(), xl::join(c.base_types));
        return false;
    }

    return true;
}


std::ostream & BidirectionalOutputStreamProvider::get_class_collection_stream() {
    return std::cerr;
}

ostream & BidirectionalOutputStreamProvider::get_class_stream(WrappedClass const & c) {
    this->output_file.close();
    this->output_file.open(fmt::format("v8toolkit_generated_bidirectional_{}.h", c.get_name_alias()));

    return this->output_file;
}

BidirectionalOutputModule::BidirectionalOutputModule() :
    OutputModule(std::make_unique<BidirectionalOutputStreamProvider>())
{}



struct BidirectionalProviderContainer {


    static ProviderPtr get_provider(WrappedClass const & c) {
        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("name", c.get_name_alias()),
            std::pair("member_functions", std::ref(c.get_member_functions())),
            std::pair("includes", std::ref(c.include_files)),
            std::pair("base_name", (*c.base_types.begin())->get_name_alias()),
            std::pair("internal_params", "TODO"),
            std::pair("external_params", "TODO"),
            std::pair("internal_fields", "TODO"),
            std::pair("external_fields", "TODO")
        );
    }


    static ProviderPtr get_provider(MemberFunction const & f) {
        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("name", f.js_name),
            std::pair("comment", f.comment),
            std::pair("parameters", f.parameters),
            std::pair("return_type", f.return_type.get_name())
        );
    }


    static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("type", p.type.get_jsdoc_type_name()),
            std::pair("name", p.name)
        );
    }


    static ProviderPtr get_provider(ClassFunction::TypeInfo const & t) {
        return xl::templates::make_provider<BidirectionalProviderContainer>("Implement me");

    }
};


void BidirectionalOutputModule::process(std::vector < WrappedClass const*> wrapped_classes)
{

    BidirectionalOutputStreamProvider stream_provider;

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Starting Bidirectional output module");

    auto templates = xl::templates::load_templates("bidirectional_templates");

    for(auto c : wrapped_classes) {
        auto & ostream = this->output_stream_provider->get_class_stream(*c);

        ostream << templates["class"].fill<BidirectionalProviderContainer>(std::ref(*c), &templates);
    }

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Finished Bidirectional output module");

}

string BidirectionalOutputModule::get_name() {
    return "BidirectionalOutputModule";
}

OutputCriteria & BidirectionalOutputModule::get_criteria() {
    return this->criteria;
}


}