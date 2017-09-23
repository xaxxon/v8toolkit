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
        os << "STUFF" << std::endl;
    }

    void end() override {
        ClassOutputModule::end();
    }
};


class JavascriptStubOutput : public ClassCollectionHandler {
public:

    JavascriptStubOutput(StreamCreatorCallback callback) : ClassCollectionHandler(callback) {
    }

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
 *   more than once will return the cached result, not re-execute the source.
 * @param {String} module_name name of the module to require
 */
function require(module_name){}


)JS_API_HEADER";



}