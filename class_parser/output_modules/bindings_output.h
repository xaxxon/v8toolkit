// one file for N classes

#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser::bindings_output {


// returns whether a WrappedClass object should be part of the JavaScript stub
struct BindingsCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c);
};


class BindingsOutputStreamProvider : public OutputStreamProvider {
private:
    std::stringstream string_stream;
    int count = 0;
    std::ofstream output_stream;
public:
    std::ostream & get_class_collection_stream() override;
    ~BindingsOutputStreamProvider()
    {}
};



class BindingsOutputModule : public OutputModule {
protected:
    size_t max_declarations_per_file = 0; // default to unlimited
    BindingsCriteria criteria;
public:
    /**
    * Creates a new object for outputing bindings commands for compilation into a v8toolkit program
    * @param max_declarations_per_file how many declaration "points" go into one file before the next one is started
    */
    BindingsOutputModule(size_t max_declarations_per_file = -1,
                         std::unique_ptr<OutputStreamProvider> output_stream_provider =
                         std::make_unique<BindingsOutputStreamProvider>());

    OutputCriteria & get_criteria() override;

    void process(std::vector<WrappedClass const *> wrapped_classes) override;

    string get_name() override;

    size_t get_max_declarations_per_file() const {
        return this->max_declarations_per_file;
    }
};


} // end namespace v8toolkit::class_parser// N files to M classes