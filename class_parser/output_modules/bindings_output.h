// one file for N classes

#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser {


// returns whether a WrappedClass object should be part of the JavaScript stub
struct BindingsCriteria : public OutputCriteria {
    bool class_filter(WrappedClass const & c) override;
};


class BindingsOutputStreamProvider : public OutputStreamProvider {
private:
    std::stringstream string_stream;
    int count = 0;
    std::ofstream output_stream;
public:
    std::ostream & get_class_collection_stream() override;
    ~BindingsOutputStreamProvider(){
        std::cerr << "Generated: " << string_stream.str() << std::endl;
    }
};



class BindingsOutputModule : public OutputModule {
protected:
    size_t max_declarations_per_file = 0; // default to unlimited

public:
    /**
    * Creates a new object for outputing bindings commands for compilation into a v8toolkit program
    * @param max_declarations_per_file how many declaration "points" go into one file before the next one is started
    */
    BindingsOutputModule(size_t max_declarations_per_file) :
        max_declarations_per_file(max_declarations_per_file)
    {}

    void process(std::vector<WrappedClass const *> const & wrapped_classes) override;
};


} // end namespace v8toolkit::class_parser// N files to M classes