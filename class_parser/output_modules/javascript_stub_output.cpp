
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/templates.h>


#include "../wrapped_class.h"
#include "javascript_stub_output.h"


namespace v8toolkit::class_parser {

std::unique_ptr<xl::templates::Provider_Interface> get_provider(WrappedClass const * c) {
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


std::unique_ptr<xl::templates::Provider_Interface> get_provider(DataMember const & d) {
    return xl::templates::make_provider(
        std::pair("comment", d.comment),
        std::pair("name", d.js_name),
        std::pair("type", d.type.get_jsdoc_type_name())
    );
}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(ConstructorFunction const & f) {
    return xl::templates::make_provider("BOGUS CONSTRUCTOR PROVIDER");
}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(MemberFunction const & f) {
    return xl::templates::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters)),
        std::pair("return_type_name", xl::templates::make_provider(f.return_type.get_jsdoc_type_name())),
        std::pair("return_comment", f.return_type_comment)
    );
}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(StaticFunction const & f) {
    return xl::templates::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", xl::templates::make_provider(f.parameters)),
        std::pair("return_type_name", xl::templates::make_provider(f.return_type.get_jsdoc_type_name())),
        std::pair("return_comment", f.return_type_comment)
    );
}



std::unique_ptr<xl::templates::Provider_Interface> get_provider(ClassFunction::ParameterInfo const & p) {
    return xl::templates::make_provider(
        std::pair("type", p.type.get_jsdoc_type_name()),
        std::pair("name", p.name),
        std::pair("comment", p.description)
    );
}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(ClassFunction::TypeInfo const & t) {
    return xl::templates::make_provider("Implement me");

}


void JavascriptStubOutputModule::process(std::vector<WrappedClass const *> const & wrapped_classes) {
    auto templates = xl::templates::load_templates("javascript_stub_templates");

    auto result = templates["file"].fill(xl::templates::Provider(std::pair("classes", wrapped_classes)), templates);
//        auto result = templates["file"].fill(xl::templates::Provider(std::pair("classes", [&wrapped_classes]()->auto&{return wrapped_classes;})), templates);

    std::cerr << fmt::format("result: {}", result) << std::endl;
}


} // end namespace v8toolkit::class_parser