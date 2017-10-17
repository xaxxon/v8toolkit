// one file for N classes

#pragma once

#include <vector>
#include <fstream>
#include <iostream>

#include <xl/template.h>

#include "../output_modules.h"



namespace v8toolkit::class_parser {



// defined below - just too big to have here
extern std::string js_api_header;


class JavascriptStubMemberFunctionVisitor {
private:
    std::ostream & os;

public:
    JavascriptStubMemberFunctionVisitor(std::ostream & os) : os(os) {}
    void operator()(MemberFunction const & member_function) {

      static xl::Template tmpl(R"(
    /**
{PARAMETERS|ClassParameterJSDocHeader|\n}
     * @return {{{JSDOC_RETURN_TYPE}}}
     */
    {JS_NAME}({JS_INPUT_PARAMETER_NAMES}){{}}
{
)");

        static xl::Template parameter_jsdoc_header_template(R"()");


//        return tmpl.fill(Provider{{{"PARAMETERS", Template("     * @param {{{}}} [{} = {}] {}")}}})

//        os << fmt::format("    /**") << endl;
//        for (auto & parameter : member_function.parameters) {
//            if (parameter.default_value != "") {
//                os << fmt::format("     * @param {{{}}} [{} = {}] {}\n", parameter.type.get_jsdoc_type_name(),
//                                      parameter.name,
//                                      parameter.default_value,
//                                      parameter.description);
//            } else {
//                os << fmt::format("     * @param {{{}}} {}\n", parameter.type.get_jsdoc_type_name(), parameter.name,
//                                      parameter.description);
//            }
//        }
//        if (!member_function.return_type.is_void()) {
//            os << fmt::format("     * @return {{{}}}", member_function.return_type.get_jsdoc_type_name()) << endl;
//        }
//
//        os << fmt::format("     */") << endl;
//        os << fmt::format("    {}({}){{}}", member_function.js_name, member_function.get_js_input_parameter_string());
//
//
//        auto tmpl = R"(
//    /**
//
//)";

    }
};


class JavascriptStubStaticFunctionVisitor {
private:
    std::ostream & os;

public:
    JavascriptStubStaticFunctionVisitor(std::ostream & os) : os(os) {}
    void operator()(StaticFunction const & static_function) {
        os << fmt::format("    /**") << endl;
        for (auto & parameter : static_function.parameters) {
            if (parameter.default_value != "") {
                os << fmt::format("     * @param {{{}}} [{} = {}] {}\n", parameter.type.get_jsdoc_type_name(),
                                  parameter.name,
                                  parameter.default_value,
                                  parameter.description);
            } else {
                os << fmt::format("     * @param {{{}}} {}\n", parameter.type.get_jsdoc_type_name(), parameter.name,
                                  parameter.description);
            }
        }
        if (!static_function.return_type.is_void()) {
            os << fmt::format("     * @return {{{}}}", static_function.return_type.get_jsdoc_type_name()) << endl;
        }
        os << fmt::format("     */") << endl;

        os << fmt::format("    static {}({}){{}}", static_function.js_name, static_function.get_js_input_parameter_string()) << endl;

    }

};

class JavascriptStubDataMemberVisitor {
private:
    std::ostream & os;

public:
    JavascriptStubDataMemberVisitor(std::ostream & os) : os(os) {}
    void operator()(DataMember const & data_member) {
        os << fmt::format(" * @property {{{}}} {} \n", data_member.type.get_jsdoc_type_name(), data_member.short_name);
    }
};



inline std::string js_api_header = R"JS_API_HEADER(


)JS_API_HEADER";



// returns whether a WrappedClass object should be part of the JavaScript stub
class JavascriptStubCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c) {
        cerr << "Checking class criteria" << endl;

        if (c.get_name_alias().find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
                                     c.get_name_alias()) << std::endl;
            return false;
        } else if (c.base_types.size() > 0 && (*c.base_types.begin())->get_name_alias().find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
                                     c.get_name_alias(),
                                     (*c.base_types.begin())->get_name_alias()) << std::endl;
            return false;
        } else if (c.bidirectional) {
            std::cerr << fmt::format("Skipping generation of js stub for {} because it's a bidirectional type", c.get_name_alias()) << std::endl;
            return false;
        }

        return true;
    }
};

class JavascriptStubOutputStreamProvider : public OutputStreamProvider {
private:
    std::stringstream string_stream;
public:
    virtual std::ostream & get_class_collection_stream() {
//        return this->string_stream;
        return std::cerr;
    }
    ~JavascriptStubOutputStreamProvider(){
        std::cerr << "Generated: " << string_stream.str() << std::endl;
    }
};



}