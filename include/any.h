#pragma once

#include <string>

#include "v8helpers.h"

namespace v8toolkit {

/**
 * Class used to store an object of an arbitrary type and allow runtime querying of the type contained
 * via dyanmic_cast of the AnyBase object to an AnyPtr<T> object.
 */
struct AnyBase {
    virtual ~AnyBase();

    std::string type_name;

    AnyBase(const std::string type_name)
        : type_name(std::move(type_name)) {}


};


/**
 * Holds a pointer to an object that is either a T or derived from T.  If an AnyBase object can by
 * dynamic_cast'd to this type, then it is known that the pointer is compatible with type T.
 */
template<class T>
struct AnyPtr : public AnyBase {
    static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value, "AnyPtr must be specified with a non-pointer/reference type");

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