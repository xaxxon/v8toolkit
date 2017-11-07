
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
        return false;
    }

    return true;
}


static ProviderPtr get_provider(WrappedClass const & c) {
    return xl::templates::make_provider(
        std::pair("name", c.get_name_alias()),
        std::pair("member_functions", std::ref(c.get_member_functions()))
    );
}


static ProviderPtr get_provider(MemberFunction const & f) {
    return xl::templates::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters)),
        std::pair("return_type", f.return_type.get_name())
    );
}


static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
    return xl::templates::make_provider(
        std::pair("type", p.type.get_jsdoc_type_name()),
        std::pair("name", p.name)
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