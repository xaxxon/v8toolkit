
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
    void process(std::vector<WrappedClass const *> const & wrapped_classes) override {};
};


} // end namespace v8toolkit::class_parser::noop_output

