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
    virtual bool operator()(WrappedClass const &) {return true;}

    /**
     * Does the class function meet the criteria?
     * @return whether the class function meets the criteria of this object or not
     */
    virtual bool operator()(ClassFunction const &) {return true;}

    /**
     * Does the class data member meet the criteria?
     * @return whether the class data member meets the criteria of this object or not
     */
    virtual bool operator()(DataMember const &) {return true;}
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
 * Stores a reference to an external string stream which can be inspected after the tool has run
 */
class StringStreamOutputStreamProvider : public OutputStreamProvider {
public:
    std::stringstream & string_stream;

    StringStreamOutputStreamProvider(std::stringstream & string_stream) : string_stream(string_stream) {}
    std::ostream & get_class_collection_stream() override {
        return string_stream;
    }

};

/**
 * Process the set of classes which are to be wrapped
 */
class OutputModule {
public:
    std::unique_ptr<OutputStreamProvider> output_stream_provider;

    OutputModule(std::unique_ptr<OutputStreamProvider> output_stream_provider);

    LogWatcher<LogT> log_watcher;
    LogT::CallbackT * callback = nullptr;
    std::unique_ptr<xl::LogCallbackGuard<LogWatcher<LogT>, LogT>> log_callback_guard;
    virtual OutputCriteria & get_criteria() = 0;

    virtual ~OutputModule() = default;
    virtual void begin() {}
    void _begin();
    virtual void process(std::vector<WrappedClass const *> wrapped_classes) = 0;
    virtual void end() {}
    void _end();
    virtual string get_name() = 0;
};


} // end namespace v8toolkit::class_parser