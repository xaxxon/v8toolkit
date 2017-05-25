
#pragma once

#include "type_traits.h"
#include "wrapped_class_base.h"


namespace v8toolkit {



template<typename T, class = void>
struct CastToNative;



template<class T, class = void>
struct cast_to_native_supports_default_value : public std::false_type {};

template<class T>
struct cast_to_native_supports_default_value<T, std::void_t<std::result_of_t<CastToNative<T>(v8::Isolate *)>>> : public std::true_type {};

template<class T> constexpr bool cast_to_native_supports_default_value_v = cast_to_native_supports_default_value<T>::value;


// if a value to send to a macro has a comma in it, use this instead so it is parsed as a comma character in the value
//   and not separating another parameter to the template
#define V8TOOLKIT_COMMA ,


// add inside CastToNative::operator() to have it handle values that are functions
#define HANDLE_FUNCTION_VALUES \
    { \
	if (value->IsFunction()) { \
	    value = v8toolkit::call_simple_javascript_function(isolate, v8::Local<v8::Function>::Cast(value)); \
	} \
    }


#define CAST_TO_NATIVE_CLASS_ONLY(TYPE) \
template<> \
 struct v8toolkit::CastToNative<TYPE> {				\
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const; \
    static constexpr bool callable(){return true;} /* It wouldn't be selected if it weren't callable */ \
};


#define CAST_TO_NATIVE_CODE(TYPE, CODE) \
    TYPE CastToNative<TYPE>::operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const CODE


#define CAST_TO_NATIVE(TYPE, CODE) \
    CAST_TO_NATIVE_CLASS_ONLY(TYPE) \
    inline CAST_TO_NATIVE_CODE(TYPE, CODE)


} // end namespace v8toolkit
