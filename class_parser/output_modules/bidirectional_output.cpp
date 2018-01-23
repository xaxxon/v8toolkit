
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
#include "bidirectional_output.h"
#include "../helper_functions.h"

//MAY BE OTHER ISSUES WITH CONSTRUCTOR PARAMETERS


namespace v8toolkit::class_parser::bidirectional_output {

using namespace xl::templates;

extern xl::templates::Template class_template;
extern std::map<string, xl::templates::Template> bidirectional_templates;


bool BidirectionalCriteria::operator()(WrappedClass const & c) {
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


std::ostream & BidirectionalOutputStreamProvider::get_class_collection_stream() {
    return std::cerr;
}

ostream & BidirectionalOutputStreamProvider::get_class_stream(WrappedClass const & c) {
    this->output_file.close();
    if (auto results = xl::RegexPcre("^\"([^\"]+)\"$").match(c.my_include)) {
        this->output_file.open(results[1]);
        return this->output_file;
    } else {
        llvm::report_fatal_error("bidirectional type my_include field not in expected format");
    }
}




std::string generate_bidirectional_constructor_parameter_list(WrappedClass const & c) {
    auto base_type = *c.base_types.begin();
    ClassFunction bidirectional_constructor(*base_type, base_type->bidirectional_constructor);
    int param_position = 1;
    std::string result;
    result.reserve(64);
    for (auto & parameter : bidirectional_constructor.parameters) {
        result += fmt::format(", {} var{}", parameter.type.get_name(), param_position++);
    }
    return result;
}



vector<string> generate_variable_names(vector<QualType> qual_types, bool with_std_move) {
    vector<string> results;
    for (std::size_t i = 0; i < qual_types.size(); i++) {
        if (with_std_move && qual_types[i]->isRValueReferenceType()) {
            results.push_back(fmt::format("std::move(var{})", i + 1));
        } else {
            results.push_back(fmt::format("var{}", i + 1));
        }
    }
    return results;
}


vector<QualType> get_method_param_qual_types(const CXXMethodDecl * method,
                                             string const & annotation = "") {
    vector<QualType> results;
    auto parameter_count = method->getNumParams();
    for (unsigned int i = 0; i < parameter_count; i++) {
        auto param_decl = method->getParamDecl(i);


        Annotations annotations(param_decl);
        if (annotation != "" && !annotations.has(annotation)) {
            continue;
        }
        auto param_qual_type = param_decl->getType();
        results.push_back(param_qual_type);
    }
    return results;
}



struct BidirectionalProviderContainer {


    static ProviderPtr get_provider(WrappedClass const & c) {

        std::vector<MemberFunction const *> virtual_functions;
        c.foreach_inheritance_level([&](auto & current) {
            for(auto & f : current.get_member_functions()) {
                std::cerr << fmt::format("in {}, {} is virtual override? {}", current.class_name, f->name, f->is_virtual_override) << std::endl;
                if (f->is_virtual && !f->is_virtual_override) {
                    std::cerr << fmt::format("adding it") << std::endl;
                    virtual_functions.push_back(f.get());
                }
            }
        });

//
        log.info(LogSubjects::BidirectionalOutput, "virtual function count for {}: {}", c.get_js_name(), virtual_functions.size());

        log.info(LogSubjects::BidirectionalOutput, "includes for {}, {}", c.get_js_name(), xl::join(c.include_files));


        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("name", c.get_js_name()),
//            std::pair("virtual_functions", xl::erase_if(xl::copy(c.get_member_functions()), [](auto & e){return e.get()->is_virtual;})),
            std::pair("virtual_functions", virtual_functions),
            std::pair("includes", std::ref(c.include_files)),
            std::pair("base_name", (*c.base_types.begin())->class_name),
            std::pair("constructor_parameters", generate_bidirectional_constructor_parameter_list(c)),
            std::pair("constructor_variables",
                      xl::join(generate_variable_names(get_method_param_qual_types((*c.base_types.begin())->bidirectional_constructor), true)))
        );
    }


    static ProviderPtr get_provider(MemberFunction const & f) {
        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("name", f.get_short_name()),
            std::pair("js_name", f.js_name),
            std::pair("comment", f.comment),
            std::pair("params", f.parameters),
            std::pair("return_type", make_macro_safe_comma(f.return_type.get_name())),
            std::pair("param_count", std::to_string(f.parameters.size())),
            std::pair("const", f.is_const() ? "_CONST" : "")
        );
    }


    static ProviderPtr get_provider(ClassFunction::ParameterInfo const & p) {
        return xl::templates::make_provider<BidirectionalProviderContainer>(
            std::pair("type", make_macro_safe_comma(p.type.get_name())),
            std::pair("name", p.name)
        );
    }


    static ProviderPtr get_provider(TypeInfo const & t) {
        return xl::templates::make_provider<BidirectionalProviderContainer>("Implement me");
    }
};



void BidirectionalOutputModule::process(std::vector < WrappedClass const*> wrapped_classes)
{

    BidirectionalOutputStreamProvider stream_provider;

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Starting Bidirectional output module");


    log.info(LogT::Subjects::BidirectionalOutput, "Bidirectional wrapped classes count: {}",
             wrapped_classes.size());

    for(auto c : wrapped_classes) {
        log.info(LogT::Subjects::BidirectionalOutput, "Creating bidirectional output for class: {}",
                 c->class_name);
        auto & ostream = this->output_stream_provider->get_class_stream(*c);

        ostream << bidirectional_templates["class"].template fill<BidirectionalProviderContainer>(std::ref(*c), &bidirectional_templates);
    }

    log.info(LogSubjects::Subjects::BidirectionalOutput, "Finished Bidirectional output module");

}

string BidirectionalOutputModule::get_name() {
    return "BidirectionalOutputModule";
}

OutputCriteria & BidirectionalOutputModule::get_criteria() {
    return this->criteria;
}





::xl::templates::Template bidirectional_class_template(R"(#pragma once

{{includes|!!
#include {{include}}}}

class {{name}} : public {{base_name}}, public v8toolkit::JSWrapper<{{base_name}}> {
public:

    {{name}}(v8::Local<v8::Context> context, v8::Local<v8::Object> object,
        v8::Local<v8::FunctionTemplate> created_by{{constructor_parameters}}) :
      {{base_name}}({{constructor_variables}}),
      v8toolkit::JSWrapper<{{base_name}}>(context, object, created_by)
    {}

{{virtual_functions|!!
    JS_ACCESS_{{param_count}}{{const}}({{return_type}}, {{<name}}, {{js_name}}{{params%%, |!{{type}}}});}}
};)");


std::map<string, Template> bidirectional_templates {
    std::pair("class", bidirectional_class_template),
};



} // end namespace v8toolkit::class_parser::bidirectional_output