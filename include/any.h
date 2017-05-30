#pragma once

#include <string>

#include "v8helpers.h"

namespace v8toolkit {

/**
* When passing an Any-type through a void *, always static_cast it to an AnyBase *
*   pointer and pass that as the void *.  This allows you to safely cast it back to
*   a AnyBase* on the other side and then dynamic_cast to any child types to
*   determine the type of the object actually stored.
*/


struct AnyBase {
    virtual ~AnyBase();

    std::string type_name;

    AnyBase(const std::string  type_name)
        : type_name(std::move(type_name)) {}


};


template<class T, class = void>
struct AnyPtr;


template<class T>
struct AnyPtr<T, std::enable_if_t<!std::is_pointer<T>::value && !std::is_reference<T>::value>> : public AnyBase {
    AnyPtr(T * data) :
        AnyBase(demangle<T>()),
        data(data) {}

    T * data;

    T * get() {
        return this->data;
    }

    static T * check(AnyBase * any_base) {
        return dynamic_cast<T *>(any_base) != nullptr;
    }
};

} // end namespace v8toolkit