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

    virtual std::ostream & get_class_member_data_stream(DataMember const & data_member) {
        return this->get_class_stream(data_member.wrapped_class);
    }
};



class OutputModule_Interface {
public:
    virtual void process(std::vector<WrappedClass> const & wrapped_classes) = 0;
};


/**
 * Process the set of classes which are to be wrapped
 */
template<class OutputCriteria, class ClassVisitor, class MemberFunctionVisitor, class StaticFunctionVisitor, class DataMemberVisitor>
class OutputModule : public OutputModule_Interface{
protected:
    std::unique_ptr<OutputStreamProvider> output_stream_provider;
    std::unique_ptr<OutputCriteria> criteria;

public:
    OutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider,
                 std::unique_ptr<OutputCriteria> criteria) :
        output_stream_provider(std::move(output_stream_provider)),
        criteria(std::move(criteria))
    {}


    virtual void process(std::vector<WrappedClass> const & wrapped_classes) {

        for (WrappedClass const & wrapped_class : wrapped_classes) {
            if (wrapped_class.should_be_wrapped() && this->criteria->class_filter(wrapped_class)) {
                ClassVisitor class_visitor(wrapped_class, this->output_stream_provider->get_class_stream());

                // go through each method
                for (auto const & member_function : wrapped_class.get_member_functions()) {
                    MemberFunctionVisitor class_instance_method(*member_function,
                                                              this->output_stream_provider->get_class_function_stream(*member_function));
                }

                for (auto const & static_function : wrapped_class.get_static_functions()) {
                    MemberFunctionVisitor class_instance_method(*static_function,
                                                                this->output_stream_provider->get_class_function_stream(*static_function));
                }

                for (auto const & data_member : wrapped_class.get_members()) {
                    DataMemberVisitor(*data_member, this->output_stream_provider->get_class_member_data_stream());
                }
            }
        }
    }

};


}