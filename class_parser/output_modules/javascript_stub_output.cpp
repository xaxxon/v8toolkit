
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
        std::pair("data_members", xl::templates::make_provider(c->get_members()))

    );
}

std::unique_ptr<xl::templates::Provider_Interface> get_provider(std::unique_ptr<DataMember> const & d) {
    return xl::templates::make_provider(
        std::pair("comment", d->comment),
        std::pair("name", d->js_name),
        std::pair("type", d->type.get_jsdoc_type_name())
    );

}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(ClassFunction const * f) {
    return xl::templates::make_provider(
        std::pair("name", f->js_name),
        std::pair("static", f->is_static ? "static" : ""),
        std::pair("parameters", xl::templates::make_provider(f->parameters)),
        std::pair("return", xl::templates::make_provider(f->return_type))
    );
}


std::unique_ptr<xl::templates::Provider_Interface> get_provider(ClassFunction::ParameterInfo const & p) {
    return xl::templates::make_provider("BOGUS PARAMETERINFO PROVIDER");
}

std::unique_ptr<xl::templates::Provider_Interface> get_provider(ClassFunction::TypeInfo const & p) {
    return xl::templates::make_provider("BOGUS TYPEINFO PROVIDER");

}

void JavascriptStubOutputModule::process(std::vector<WrappedClass const *> const & wrapped_classes) {
    auto templates = xl::templates::load_templates("javascript_stub_templates");

    auto result = templates["file"].fill(xl::templates::Provider(std::pair("classes", wrapped_classes)), templates);
//        auto result = templates["file"].fill(xl::templates::Provider(std::pair("classes", [&wrapped_classes]()->auto&{return wrapped_classes;})), templates);

    std::cerr << fmt::format("result: {}", result) << std::endl;
}


} // end namespace v8toolkit::class_parser