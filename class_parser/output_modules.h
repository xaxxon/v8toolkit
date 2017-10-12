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

    virtual std::ostream & get_class_function_stream() {
        return this->get_class_collection_stream();
    }

    virtual std::ostream & get_class_member_data_stream() {
        return this->get_class_collection_stream();
    }
};



class OutputModule_Interface {
public:
    virtual void process(std::vector<WrappedClass> const & wrapped_classes) = 0;
};

class NoOpClassVisitor{
    NoOpClassVisitor(std::ostream &) {}
    void operator()(WrappedClass const &) {}
};

class NoOpMemberFunctionVisitor {
    NoOpMemberFunctionVisitor(std::ostream &) {}
    void operator()(MemberFunction const &) {}
};

class NoOpStaticFunctionVisitor {
    NoOpStaticFunctionVisitor(std::ostream &) {}
    void operator()(StaticFunction const &) {}
};

class NoOpDataMemberVisitor {
    NoOpDataMemberVisitor(std::ostream &) {}
    void operator()(DataMember const &) {}
};


/**
 * Process the set of classes which are to be wrapped
 * @tparam OutputCriteria
 * @tparam ClassVisitor takes an ostream and a WrappedClass and prints and prints any class headers from constructor and any class footers from destructor
 * @tparam MemberFunctionVisitor takes an ostream and a member function and prints the member function from the constructor
 * @tparam StaticFunctionVisitor takes an ostream and a static function and prints the static function from the constructor
 * @tparam DataMemberVisitor takes an ostream and a data member and prints the data member from the constructor
 */
template<
    class ClassVisitor = NoOpClassVisitor,
    class MemberFunctionVisitor = NoOpMemberFunctionVisitor,
    class StaticFunctionVisitor = NoOpStaticFunctionVisitor,
    class DataMemberVisitor = NoOpDataMemberVisitor>
class OutputModule : public OutputModule_Interface{
protected:
    std::unique_ptr<OutputStreamProvider> output_stream_provider;
    std::unique_ptr<OutputCriteria> criteria;

public:
    OutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider, std::unique_ptr<OutputCriteria> criteria) :
        output_stream_provider(std::move(output_stream_provider)),
        criteria(std::move(criteria))
    {}


    virtual void process(std::vector<WrappedClass> const & wrapped_classes) {

        for (WrappedClass const & wrapped_class : wrapped_classes) {
            if (wrapped_class.should_be_wrapped() && this->criteria->class_filter(wrapped_class)) {

            }
        }
    }

};


}