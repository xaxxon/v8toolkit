#pragma once

#include <vector>


#include "wrapped_class.h"


namespace v8toolkit::class_parser {


class ParsedMethod;



class ClassOutputModule {
protected:
    std::ostream & os;
public:
    ClassOutputModule(std::ostream & os) : os(os) {}

    virtual ~ClassOutputModule() {}

    virtual void begin() {}

    virtual void process(WrappedClass const & c) = 0;

    virtual void end() {}

};



class ClassCollectionHandler {
protected:
    std::unique_ptr<std::ostream> os_pointer;
    std::ostream & os;
public:
    using StreamCreatorCallback = std::function<std::unique_ptr<std::ostream>()>;

    ClassCollectionHandler(StreamCreatorCallback callback) : os_pointer(callback()), os(*os_pointer) {}

    virtual std::unique_ptr<ClassOutputModule> make_class_output_module() = 0;

    virtual void begin() {}

    virtual void process(std::vector<WrappedClass> const & wrapped_classes) {
        for (auto const & wrapped_class : wrapped_classes) {
            auto class_output_module = this->make_class_output_module();
            class_output_module->process(wrapped_class);
        }
    }

    virtual void end() {}
};



class MethodOutputModule {
private:
    std::ostream & os;
public:
    MethodOutputModule(std::ostream & os) : os(os) {}

    virtual ~MethodOutputModule() {}

    virtual void begin() {}

    virtual void process(ParsedMethod const & c) = 0;

    virtual void end() {}
};


class StaticMethodOutputModule {
private:
    std::ostream & os;
public:
    StaticMethodOutputModule(std::ostream & os);

    virtual ~StaticMethodOutputModule() {}

    virtual void begin();

    virtual void process(ParsedMethod const & c);

    virtual void end();
};

}