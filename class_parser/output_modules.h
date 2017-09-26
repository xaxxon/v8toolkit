#pragma once

#include <vector>


#include "wrapped_class.h"
#include "parsed_method.h"


namespace v8toolkit::class_parser {




class OutputCriteria {
public:
    virtual ~OutputCriteria(){}

    /**
     * Does the class meet the criteria?
     * @return whether the class meets the criteria of this object or not
     */
    virtual bool class_filter(WrappedClass const &) {return true;}

    /**
     * Does the class function meet the criteria?
     * @return whether the class function meets the criteria of this object or not
     */
    virtual bool class_function_filter(ClassFunction const &) {return true;}

    /**
     * Does the class data member meet the criteria?
     * @return whether the class data member meets the criteria of this object or not
     */
    virtual bool class_member_data_filter(DataMember const &) {return true;}
};


/**
 * Provides the output stream for each level of the output module processing
 */
class OutputStreamProvider {
public:
    virtual ~OutputStreamProvider(){}

    virtual std::ostream & get_class_collection_stream() = 0;

    virtual std::ostream & get_class_stream(WrappedClass const &) {
        return this->get_class_collection_stream();
    }

    virtual std::ostream & get_class_function_stream(ClassFunction const & class_function) {
        return this->get_class_stream(class_function.wrapped_class);
    }

    virtual std::ostream & get_class_member_data_steram(DataMember const & data_member) {
        return this->get_class_stream(data_member.wrapped_class);
    }
};






/**
 * Process the set of classes which are to be wrapped
 */
class OutputModule {
public:

    using ClassVisitor = std::function<void(WrappedClass const &, std::ostream &)>;
    using ClassFunctionVisitor = std::function<void(ClassFunction const &, std::ostream &)>;
    using DataMemberVisitor = std::function<void(DataMember const &, std::ostream &)>;

protected:
    std::unique_ptr<OutputStreamProvider> output_stream_provider;
    std::unique_ptr<OutputCriteria> criteria;

    ClassVisitor & class_visitor;
    ClassFunctionVisitor & class_function_visitor;
    DataMemberVisitor & data_member_visitor;

public:
    OutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider,
                 ClassVisitor & class_visitor,
                 ClassFunctionVisitor & class_function_visitor,
                 DataMemberVisitor & data_member_visitor) :
        output_stream_provider(std::move(output_stream_provider)),
        class_visitor(class_visitor),
        class_function_visitor(class_function_visitor),
        data_member_visitor(data_member_visitor)
    {}


    virtual void process(std::vector<WrappedClass> const & wrapped_classes) {

        for (WrappedClass const & wrapped_class : wrapped_classes) {
            if (wrapped_class.should_be_wrapped() && this->criteria->class_filter(wrapped_class)) {
                class_visitor(wrapped_class, output_stream_provider->get_class_collection_stream());

                // go through each method
                for (auto const & class_function : wrapped_class.get_member_functions()) {
                    this->class_function_visitor(*class_function, this->output_stream_provider->get_class_function_stream(*class_function));
                }

                // go through each data member
            }
        }
    }

};


}