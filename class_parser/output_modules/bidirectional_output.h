// one file for N classes

#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser::bidirectional_output {


// returns whether a WrappedClass object should be part of the JavaScript stub
class BidirectionalCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c);
};



class BidirectionalOutputStreamProvider : public OutputStreamProvider {
private:
//    std::stringstream string_stream;
    std::ofstream output_file;
public:
    std::ostream & get_class_collection_stream() override;

    ostream & get_class_stream(WrappedClass const & c) override;


    ~BidirectionalOutputStreamProvider()
    {}
};



class BidirectionalOutputModule : public OutputModule {
private:
    BidirectionalCriteria criteria;
public:

    std::stringstream string_stream;

    BidirectionalOutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider =
        std::make_unique<BidirectionalOutputStreamProvider>()) :
        OutputModule(std::move(output_stream_provider))
    {}

    void process(std::vector<WrappedClass const *> wrapped_classes) override;

    string get_name() override;

    OutputCriteria & get_criteria() override;
};


} // end namespace v8toolkit::class_parser