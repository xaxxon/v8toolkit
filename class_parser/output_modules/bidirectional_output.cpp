
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/templates.h>
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
    }


    return true;
}



static ProviderPtr get_provider(WrappedClass const & c) {
    return xl::templates::make_provider(
        std::pair("comment", c.comment),
        std::pair("name", c.get_name_alias()),
        std::pair("data_members", std::ref(c.get_members())),
        std::pair("constructors", std::ref(c.get_constructors())),
        std::pair("member_functions", std::ref(c.get_member_functions())),
        std::pair("static_functions", std::ref(c.get_static_functions())),
        std::pair("inheritance", fmt::format("{}", c.base_types.empty() ? "" : (*c.base_types.begin())->get_name_alias()))
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


void BidirectionalOutputModule::process(std::vector < WrappedClass const*> wrapped_classes)
{

}

string BidirectionalOutputModule::get_name() {
    return "BidirectionalOutputModule";
}

OutputCriteria & BidirectionalOutputModule::get_criteria() {
    return this->criteria;
}


}