#pragma once

#include <vector>
#include <fstream>
#include <iostream>

#include "../output_modules.h"


namespace v8toolkit::class_parser {

// returns whether a WrappedClass object should be part of the JavaScript stub
class JavascriptStubCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c) {
        cerr << "Checking class criteria" << endl;

        if (c.name_alias.find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
                                     c.name_alias) << std::endl;
            return false;
        } else if (c.base_types.size() > 0 && (*c.base_types.begin())->name_alias.find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
                                     c.name_alias,
                                     (*c.base_types.begin())->name_alias) << std::endl;
            return false;
        } else if (c.bidirectional) {
            std::cerr << fmt::format("Skipping generation of js stub for {} because it's a bidirectional type", c.name_alias) << std::endl;
            return false;
        }

        return true;
    }
};

class JavascriptStubOutputStreamProvider : public OutputStreamProvider {
private:
    std::stringstream string_stream;
    virtual std::ostream & get_class_collection_stream() {
        return this->string_stream;
    }
};




extern std::string js_api_header;

class JavascriptStubClassVisitor {
private:
    std::ostream & os;
public:

    JavascriptStubClassVisitor(std::ostream & os) : os(os){}

    void operator()(WrappedClass const & c) {


        cerr << fmt::format("Generating js stub for {}", c.name_alias) << endl;


        string indentation = "    ";

        os << "/**\n";
        os << fmt::format(" * @class {}\n", c.name_alias);

        c.get_enums();
        for (auto & member : c.get_members()) {
            os << member->get_js_stub();
        }
        os << fmt::format(" **/\n", indentation);


        os << fmt::format("class {}", c.name_alias);

        if (c.base_types.size() == 1) {
            os << fmt::format(" extends {}", (*c.base_types.begin())->name_alias);
        }
        os << " {\n\n";

//        // not sure what to do if there are multiple constructors...
//        bool first_method = true;
//        for (auto & constructor : c.get_constructors()) {
//            if (!first_method) {
//                os << ",";
//            }
//            first_method = false;
//
//            os << endl << endl;
//            os << constructor->generate_js_stub();
//        }

//        std::cerr << fmt::format("generating stub for {} methods", c.get_member_functions().size()) << std::endl;
//        for (auto & method : c.get_member_functions()) {
//            os << std::endl << method->generate_js_stub() << std::endl;
//        }
//
//
//        std::cerr << fmt::format("generating stub for {} static methods", c.get_static_functions().size()) << std::endl;
//        for (auto & method : c.get_static_functions()) {
//            os << std::endl << method->generate_js_stub() << std::endl;
//        }


    }

    ~JavascriptStubClassVisitor(){
        os << fmt::format("\n}}\n");
    }
};


class JavascriptStubMemberFunctionVisitor {
private:
    std::ostream & os;

public:
    JavascriptStubMemberFunctionVisitor(std::ostream & os) : os(os) {}
    void operator()(MemberFunction const & member_function) {
        os << fmt::format("    /**") << endl;
        for (auto & parameter : member_function.parameters) {
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
        if (!member_function.return_type.is_void()) {
            os << fmt::format("     * @return {{{}}}", member_function.return_type.get_jsdoc_type_name()) << endl;
        }

        os << fmt::format("     */") << endl;
        os << fmt::format("    {}({}){{}}", member_function.js_name, member_function.get_js_input_parameter_string());

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

/**
 * @type World
 */
var world;

/**
 * @type Map
 */
var map;

/**
 * @type Game
 */
var game;

/**
 * Prints a string and appends a newline
 * @param {String} s the string to be printed
 */
function println(s){}

/**
 * Prints a string without adding a newline to the end
 * @param {String} s the string to be printed
 */
function print(s){}

/**
 * Dumps the contents of the given variable - only 'own' properties
 * @param o {Object} the variable to be dumped
 */
function printobj(o){}

/**
 * Dumps the contents of the given variable - all properties including those of prototype chain
 * @param o {Object} the variable to be dumped
 */
function printobjall(o){}

/**
 * Attempts to load the given module and returns the exported data.  Requiring the same module
 *   more than once will return the cached os, not re-execute the source.
 * @param {String} module_name name of the module to require
 */
function require(module_name){}


)JS_API_HEADER";



}