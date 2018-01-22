
#pragma once

#include <vector>
#include <fstream>
#include <iostream>


#include "../output_modules.h"


namespace v8toolkit::class_parser::javascript_subclass_template_output {


// returns whether a WrappedClass object should be part of the JavaScript stub
class JavascriptSubclassTemplateCriteria : public OutputCriteria {
    bool operator()(WrappedClass const & c);
};



class JavascriptSubclassTemplateOutputStreamProvider : public OutputStreamProvider {
private:
    std::ofstream output_file;
public:
    std::ostream & get_class_collection_stream() override;

    ostream & get_class_stream(WrappedClass const & c) override;


    ~JavascriptSubclassTemplateOutputStreamProvider()
    {}
};



class JavascriptSubclassTemplateOutputModule : public OutputModule {
private:
    JavascriptSubclassTemplateCriteria criteria;
public:

    std::stringstream string_stream;

    JavascriptSubclassTemplateOutputModule(
        std::unique_ptr<OutputStreamProvider> output_stream_provider =
        std::make_unique<JavascriptSubclassTemplateOutputStreamProvider>()) :

        OutputModule(std::move(output_stream_provider))
    {}

    void process(std::vector<WrappedClass const *> wrapped_classes) override;

    string get_name() override;

    OutputCriteria & get_criteria() override;
};


} // end namespace v8toolkit::class_parser