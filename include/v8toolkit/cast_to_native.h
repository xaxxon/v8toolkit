
#pragma once

#include "type_traits.h"
#include "wrapped_class_base.h"


namespace v8toolkit {

struct CastToNativeDefaultBehavior;


template<typename T, typename Behavior = CastToNativeDefaultBehavior, typename = void>
struct CastToNative; // has full implementation in cast_to_native_impl.h

/**
 * Subclass this type to specialize specific calls for CastToNative differently than the default implementations.
 * The call chain of CastToNative will prefer specializations matching the behavior but fall back to CastToNative
 * impleementations where no behavior-specific specialization is present
 * @tparam Derived CRTP type deriving from this 
 */
template<typename Derived>
struct CastToNativeBehaviorBase {
    template<typename T, typename Behavior = Derived>
    auto operator()(v8::Local<v8::Value> t) {
        return CastToNative<T, Behavior>()(v8::Isolate::GetCurrent(), t);
    }
};


struct CastToNativeDefaultBehavior : CastToNativeBehaviorBase<CastToNativeDefaultBehavior> {};


template<typename T, typename = void>
struct CallableWithFunctionCallbackInfo : public std::false_type {};

template<typename T>
struct CallableWithFunctionCallbackInfo<T, std::enable_if_t<std::is_same_v<
    T, CastToNative<T>()(std::declval<v8::Isolate*>(), std::declval<v8::FunctionCallbackInfo<v8::Value> const &>())
>>> : public std::true_type {};

template<typename T>
constexpr bool CallableWithFunctionCallbackInfo_v = CallableWithFunctionCallbackInfo<T>::value;


template<typename T, typename = void>
struct cast_to_native_supports_default_value : public std::false_type {};

template<typename T>
struct cast_to_native_supports_default_value<T, std::void_t<std::result_of_t<CastToNative<T>(v8::Isolate *)>>> : public std::true_type {};

template<typename T> constexpr bool cast_to_native_supports_default_value_v = cast_to_native_supports_default_value<T>::value;


// if a value to send to a macro has a comma in it, use this instead so it is parsed as a comma character in the value
//   and not separating another parameter to the template
#define V8TOOLKIT_COMMA ,




#define CAST_TO_NATIVE_CLASS_ONLY(TYPE) \
template<typename Behavior> \
 struct v8toolkit::CastToNative<TYPE, Behavior> {				\
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const; \
    static constexpr bool callable(){return true;} /* It wouldn't be selected if it weren't callable */ \
};


#define CAST_TO_NATIVE_CODE(TYPE, CODE) \
    template<typename Behavior> \
    TYPE CastToNative<TYPE, Behavior>::operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const CODE


#define CAST_TO_NATIVE(TYPE, CODE) \
    CAST_TO_NATIVE_CLASS_ONLY(TYPE) \
    CAST_TO_NATIVE_CODE(TYPE, CODE)


} // end namespace v8toolkit
