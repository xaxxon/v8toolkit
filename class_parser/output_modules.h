#pragma once

#include <vector>


#include "wrapped_class.h"


namespace v8toolkit::class_parser {


class ParsedMethod;


class ClassCriteria {
public:
    virtual ~ClassCriteria(){}

    /**
     * Does the class meet the criteria?
     * @return whether the class meets the criteria of this object or not
     */
    virtual bool operator()(WrappedClass const &) = 0;
};



// returns whether a WrappedClass object should be part of the JavaScript stub
class JavascriptStubClassCriteria : public ClassCriteria {
    bool operator()(WrappedClass const & c) {
        if (c.name_alias.find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it has template syntax",
                                     c.name_alias) << std::endl;
            return false;
        } else if (c.base_types.size() > 0 && (*c.base_types.begin())->name_alias.find("<") != std::string::npos) {
            std::cerr << fmt::format("Skipping generation of stub for {} because it extends a type with template syntax ({})",
                                     c.name_alias,
                                     (*c.base_types.begin())->name_alias) << std::endl;
            return false;
        }

        return true;
    }
};


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
    std::unique_ptr<ClassCriteria> criteria;
    std::ostream & os;
public:
    using StreamCreatorCallback = std::function<std::unique_ptr<std::ostream>()>;

    ClassCollectionHandler(StreamCreatorCallback callback, std::unique_ptr<ClassCriteria> criteria) :
        os_pointer(callback()),
        criteria(std::move(criteria)),
        os(*os_pointer) {}

    virtual std::unique_ptr<ClassOutputModule> make_class_output_module() = 0;

    virtual void begin() {}

    virtual void process(std::vector<WrappedClass> const & wrapped_classes) {
        for (auto const & wrapped_class : wrapped_classes) {
            if (this->criteria->operator()(wrapped_class)) {
                auto class_output_module = this->make_class_output_module();
                class_output_module->process(wrapped_class);
            }
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