
#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser::javascript_stub_output {


// returns whether a WrappedClass object should be part of the JavaScript stub
class JavascriptStubCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c) {
//        cerr << "Checking class criteria" << endl;

        if (c.bidirectional) {
            log.info(LogSubjects::Subjects::JavaScriptStubOutput, "Skipping generation of js stub for {} because it's a bidirectional type", c.get_name_alias());
            return false;
        }

        return true;
    }
};

class JavascriptStubOutputStreamProvider : public OutputStreamProvider {
private:
    std::unique_ptr<ostream> output_stream;
public:
    virtual std::ostream & get_class_collection_stream();
    ~JavascriptStubOutputStreamProvider();

};




class JavascriptStubOutputModule : public OutputModule {
private:
    JavascriptStubCriteria criteria;
public:
    JavascriptStubOutputModule();
    JavascriptStubOutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider);
    void process(std::vector<WrappedClass const *> wrapped_classes) override;

    OutputCriteria & get_criteria() override;

    string get_name() override;
};


} // end namespace v8toolkit::class_parser::javascript_stub_output

