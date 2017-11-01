
#include <iostream>
#include <fmt/ostream.h>
#include <vector>

#include <xl/templates.h>
using namespace xl::templates;


#include "../wrapped_class.h"
#include "javascript_stub_output.h"


namespace v8toolkit::class_parser {

std::ostream & JavascriptStubOutputStreamProvider::get_class_collection_stream() {
    if (!this->output_stream) {
        this->output_stream = std::make_unique<std::ofstream>("js-api.js");
    }
    return *this->output_stream;
}

JavascriptStubOutputStreamProvider::~JavascriptStubOutputStreamProvider() {
}


struct JavascriptStubProviderContainer {

    using P = xl::templates::DefaultProviders<JavascriptStubProviderContainer>;

static ProviderPtr get_provider(WrappedClass const & c) {
    using P = xl::templates::DefaultProviders<JavascriptStubProviderContainer>;
//    std::cerr << fmt::format("js stub output wrapped class provider") << std::endl;

    return P::make_provider(
        std::pair("comment", c.comment),
        std::pair("name", c.get_name_alias()),
        std::pair("data_members", P::make_provider(c.get_members())),
        std::pair("constructors", P::make_provider(c.get_constructors())),
        std::pair("member_functions", P::make_provider(c.get_member_functions())),
        std::pair("static_functions", P::make_provider(c.get_static_functions())),
        std::pair("inheritance", fmt::format("{}", c.base_types.empty() ? "" : " extends " + (*c.base_types.begin())->get_name_alias()))
    );
}


static ProviderPtr get_provider(DataMember const & d) {
    return P::make_provider(
        std::pair("comment", d.comment),
        std::pair("name", d.js_name),
        std::pair("type", d.type.get_jsdoc_type_name())
    );
}


static ProviderPtr get_provider(ConstructorFunction const & f) {
    return P::make_provider(
        std::pair("comment", f.comment),
        std::pair("parameters", P::make_provider(f.parameters))
    );
}


static ProviderPtr get_provider(MemberFunction const & f) {
    return P::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", P::make_provider(f.parameters)),
        std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
        std::pair("return_comment", f.return_type_comment)
    );
}


static ProviderPtr get_provider(StaticFunction const & f) {
    return P::make_provider(
        std::pair("name", f.js_name),
        std::pair("comment", f.comment),
        std::pair("parameters", P::make_provider(f.parameters)),
        std::pair("return_type_name", f.return_type.get_jsdoc_type_name()),
        std::pair("return_comment", f.return_type_comment)
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



void JavascriptStubOutputModule::process(std::vector<WrappedClass const *> const & wrapped_classes) {
//    std::cerr << fmt::format("making js stub output") << std::endl;
    auto templates = xl::templates::load_templates("javascript_stub_templates");

    auto result = templates["file"].fill<JavascriptStubProviderContainer>(make_provider<JavascriptStubProviderContainer>(std::pair("classes", wrapped_classes)), templates);
//        auto result = templates["file"].fill(xl::templates::Provider(std::pair("classes", [&wrapped_classes]()->auto&{return wrapped_classes;})), templates);

    std::cerr << result << std::endl;
    stream_provider.get_class_collection_stream() << result << std::endl;
}


} // end namespace v8toolkit::class_parser