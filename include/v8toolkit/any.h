#pragma once

#include <string>

#include <xl/demangle.h>

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

    template <typename T>
    T const * get() const;

    template <typename T>
    T * get();

};


/**
 * Holds a pointer to an object that is either a T or derived from T.  If an AnyBase object can by
 * dynamic_cast'd to this type, then it is known that the pointer is compatible with type T.
 */
template<class T>
struct AnyPtr : public AnyBase {
private:
    static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value, "AnyPtr must be specified with a non-pointer/reference type");

    T * data;

public:
    AnyPtr(T * data) :
        AnyBase(xl::demangle<T>()),
        data(data) {}


    /**
     * Returns the stored pointer to the object of type T
     * @return
     */
    T * get() const {
        return this->data;
    }

    /**
     * Returns the object of type T if it is compatible with T or nullptr otherwise
     * @param any_base AnyBase object to test to see if it contains an object of type T
     * @return Type T object if contained object is compatible or nullptr if not
     */
    static T * check(AnyBase * any_base) {
         return dynamic_cast<T *>(any_base);
    }
};


template <typename T>
T const * AnyBase::get() const {
    if (auto any = dynamic_cast<AnyPtr<T*> const *>(this)) {
        return any->get();
    }
}

template <typename T>
T * AnyBase::get() {
    if (auto any = dynamic_cast<AnyPtr<T*> *>(this)) {
        return any->get();
    }
}



} // end namespace v8toolkit