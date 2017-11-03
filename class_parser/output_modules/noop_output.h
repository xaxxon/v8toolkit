
#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser::noop_output {


/**
 * Handy little OutputModule for when you need to trick the system into running even though you don't want
 * a real OutputModule to run
 */
class NoOpOutputModule : public OutputModule {
public:
    std::stringstream string_stream;
    NoOpOutputModule():OutputModule(std::make_unique<StringStreamOutputStreamProvider>(string_stream)) {}
    void process(std::vector<WrappedClass const *> const & wrapped_classes) override {};

    string get_name() override {
        return "NoOpOutputModule";
    }
};


} // end namespace v8toolkit::class_parser::noop_output

