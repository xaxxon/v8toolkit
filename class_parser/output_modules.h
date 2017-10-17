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




/**
 * Process the set of classes which are to be wrapped
 */
class OutputModule {
public:
    virtual void process(std::vector<WrappedClass const *> const & wrapped_classes) = 0;

};


}