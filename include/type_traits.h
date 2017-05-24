
#pragma once

#include <type_traits>
#include "wrapped_class_base.h"

namespace v8toolkit {

// always returns false, in a dependent fashion
template<class T>
struct always_false : public std::false_type {
};

template<class T> constexpr bool always_false_v = always_false<T>::value;


template<class T, class = void>
struct is_wrapped_type : public std::false_type {};

template<class T>
struct is_wrapped_type<T, std::enable_if_t<std::is_base_of<v8toolkit::WrappedClassBase, T>::value>> : public std::true_type {};

template<class T>
constexpr bool is_wrapped_type_v = is_wrapped_type<T>::value;



template<class T, class = void>
struct cast_to_native_supports_default_value : public std::false_type {};

template<class T>
struct cast_to_native_supports_default_value<T, std::void_t<std::result_of_t<CastToNative<T>(v8::Isolate *)>>> : public std::true_type {};

template<class T> constexpr bool cast_to_native_supports_default_value_v = cast_to_native_supports_default_value<T>::value;

} // end namespace v8toolkit

