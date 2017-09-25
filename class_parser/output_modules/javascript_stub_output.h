#pragma once

#include <vector>
#include <fstream>
#include <iostream>

#include "../output_modules.h"


namespace v8toolkit::class_parser {


extern std::string js_api_header;

class JavascriptStubClassOutput : public ClassOutputModule {
public:

    JavascriptStubClassOutput(std::ostream & os) : ClassOutputModule(os) {}

    void begin() override {
        ClassOutputModule::begin();
    }

    void process(WrappedClass const & c) override {

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

        // not sure what to do if there are multiple constructors...
        bool first_method = true;
        for (auto & constructor : c.get_constructors()) {
            if (!first_method) {
                os << ",";
            }
            first_method = false;

            os << endl << endl;
            os << constructor->generate_js_stub();
        }

        std::cerr << fmt::format("generating stub for {} methods", c.get_member_functions().size()) << std::endl;
        for (auto & method : c.get_member_functions()) {
            os << std::endl << method->generate_js_stub() << std::endl;
        }


        std::cerr << fmt::format("generating stub for {} static methods", c.get_static_functions().size()) << std::endl;
        for (auto & method : c.get_static_functions()) {
            os << std::endl << method->generate_js_stub() << std::endl;
        }


        os << fmt::format("\n}}\n");
//    fprintf(stderr, "js stub os for class:\n%s", os.str().c_str());
    }

    void end() override {
        ClassOutputModule::end();
    }
};


class JavascriptStubOutput : public ClassCollectionHandler {
public:

    JavascriptStubOutput(std::unique_ptr<std::ostream> os) :
        ClassCollectionHandler(std::move(os), std::make_unique<JavascriptStubClassCriteria>())
    {}

    std::unique_ptr<ClassOutputModule> make_class_output_module() override {
        return std::make_unique<JavascriptStubClassOutput>(this->os);
    }


    void begin() override {
        ClassCollectionHandler::begin();
        os << js_api_header << std::endl;
    }


    void end() override {
        ClassCollectionHandler::end();
        std::cerr << "Done generating JS stub file" << std::endl;
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