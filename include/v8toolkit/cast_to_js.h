
#pragma once
#include <v8.h>

#include "type_traits.h"

namespace v8toolkit {

class JSObjectBehavior;

/**
* Casts from a native type to a boxed Javascript type
*/
template<typename T, class Behavior = JSObjectBehavior, class = void>
struct CastToJS {
    static_assert(always_false_v<T>, "Fallback CastToJS template isn't allowed - make sure header with specialization desired is included at this point");
};

/**
 * Default behavior, always call CastToJS for the entire call chain
 */
class JSObjectBehavior{
    template<class T, class Behavior = JSObjectBehavior>
    auto operator()(T && t) {
        return CastToJS<T, Behavior>()(std::forward<T>(t));
    }
};


#define CAST_TO_JS(TYPE, FUNCTION_BODY)                                        \
  template <typename T, typename Behavior>                                     \
  struct v8toolkit::CastToJS<                                                  \
      T, Behavior, std::enable_if_t<std::is_same_v<std::decay_t<T>, TYPE>>> {  \
    v8::Local<v8::Value> operator()(v8::Isolate *isolate,                      \
                                    TYPE const &value) const FUNCTION_BODY     \
  };

} // end namespace v8toolkit
