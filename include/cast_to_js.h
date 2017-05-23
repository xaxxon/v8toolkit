
#pragma once

#include "type_traits.h"

namespace v8toolkit {

/**
* Casts from a native type to a boxed Javascript type
*/

template<typename T, class = void>
struct CastToJS {
    static_assert(always_false_v<T>, "Fallback CastToJS template isn't allowed");
};


#define CAST_TO_JS(TYPE, FUNCTION_BODY)                    \
template<> \
 struct v8toolkit::CastToJS<TYPE> {                    \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const & value) const FUNCTION_BODY \
};

} // end namespace v8toolkit