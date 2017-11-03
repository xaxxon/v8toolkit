// one file for N classes

#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser {


// returns whether a WrappedClass object should be part of the JavaScript stub
class BidirectionalCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c) {
        cerr << "Checking class criteria" << endl;
//
//        if (c.get_name_alias().find("<") != std::string::npos) {
//            std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
//                                     c.get_name_alias()) << std::endl;
//            return false;
//        } else if (c.base_types.size() > 0 && (*c.base_types.begin())->get_name_alias().find("<") != std::string::npos) {
//            std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
//                                     c.get_name_alias(),
//                                     (*c.base_types.begin())->get_name_alias()) << std::endl;
//            return false;
//        } else if (c.bidirectional) {
//            std::cerr << fmt::format("Skipping generation of js stub for {} because it's a bidirectional type", c.get_name_alias()) << std::endl;
//            return false;
//        }

        return true;
    }
};

class BidirectionalOutputStreamProvider : public OutputStreamProvider {
private:
    std::stringstream string_stream;
public:
    virtual std::ostream & get_class_collection_stream() {
//        return this->string_stream;
        return std::cerr;
    }
    ~BidirectionalOutputStreamProvider(){
    }
};




class BidirectionalOutputModule : public OutputModule {
public:

    std::stringstream string_stream;
    BidirectionalOutputModule() : OutputModule(std::make_unique<StringStreamOutputStreamProvider>(string_stream))
    {}

    void process(std::vector<WrappedClass const *> const & wrapped_classes) override;

    string get_name() override;
};


} // end namespace v8toolkit::class_parser