
#pragma once
#include <v8.h>

#include "type_traits.h"

namespace v8toolkit {

/**
* Casts from a native type to a boxed Javascript type
*/

template<typename T, class = void>
struct CastToJS {
    static_assert(always_false_v<T>, "Fallback CastToJS template isn't allowed - make sure header with specialization desired is included at this point");
};


#define CAST_TO_JS(TYPE, FUNCTION_BODY)                    \
template<typename T> \
 struct v8toolkit::CastToJS<T, std::enable_if_t<std::is_same_v<std::decay_t<T>, TYPE>>> {                    \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const & value) const FUNCTION_BODY \
};

} // end namespace v8toolkit
