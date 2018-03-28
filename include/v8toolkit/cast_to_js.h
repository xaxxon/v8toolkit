
#pragma once
#include <v8.h>

#include "type_traits.h"

namespace v8toolkit {

struct CastToJSDefaultBehavior;

/**
* Casts from a native type to a boxed Javascript type
*/
template<typename T, class Behavior = CastToJSDefaultBehavior, class = void>
struct CastToJS {
    static_assert(always_false_v<T>, "Fallback CastToJS template isn't allowed - make sure header with specialization desired is included at this point");
};


/**
 * Subclass this type to specialize specific calls for CastToJS differently than the default implementations.
 * The call chain of CastToJS will prefer specializations matching the behavior but fall back to CastToJS 
 * impleementations where no behavior-specific specialization is present
 * @tparam Derived CRTP type deriving from this 
 */
template<typename Derived>
struct CastToJSBehaviorBase{
    template<class T, class Behavior = Derived>
    auto operator()(T && t) {
        return CastToJS<T, Behavior>()(v8::Isolate::GetCurrent(), std::forward<T>(t));
    }
};


/**
 * Default behavior, always call CastToJS for the entire call chain
 */
struct CastToJSDefaultBehavior : CastToJSBehaviorBase<CastToJSDefaultBehavior> {};


#define CAST_TO_JS(TYPE, FUNCTION_BODY)                                        \
  template <typename T, typename Behavior>                                     \
  struct v8toolkit::CastToJS<                                                  \
      T, Behavior, std::enable_if_t<std::is_same_v<std::decay_t<T>, TYPE>>> {  \
    v8::Local<v8::Value> operator()(v8::Isolate *isolate,                      \
                                    TYPE const &value) const FUNCTION_BODY     \
  };

} // end namespace v8toolkit
