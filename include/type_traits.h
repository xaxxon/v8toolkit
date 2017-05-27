
#pragma once

#include <type_traits>
#include <string_view>

#include "wrapped_class_base.h"

namespace v8toolkit {

// always returns false, in a dependent fashionf
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


// Is T a specialization of std::basic_string_view?
template<class T>
struct is_string_view : public std::false_type {};

template<class CharT, class Traits>
struct is_string_view<std::basic_string_view<CharT, Traits>> : public std::true_type {};

template<class T>
constexpr bool is_string_view_v = is_string_view<T>::value;


// is T a type that refers to a string but is not responsible for the memory associated with the string
template<class T, class = void>
struct is_string_not_owning_memory : public std::false_type {};

template<class T>
struct is_string_not_owning_memory<T, std::enable_if_t<
    std::is_same_v<T, char *> ||
    std::is_same_v<T, char const *> ||
    is_string_view_v<T>
    >> : public std::true_type {};

template<class T>
constexpr bool is_string_not_owning_memory_v = is_string_not_owning_memory<T>::value;

template<class T>
struct is_v8_local : public std::false_type {};

template<class T>
struct is_v8_local<v8::Local<T>> : public std::true_type {};

template<class T>
constexpr bool is_v8_local_v = is_v8_local<T>::value;

template<class T>
struct is_v8_global : public std::false_type {};

template<class T>
struct is_v8_global<v8::Global<T>> : public std::true_type {};

template<class T>
constexpr bool is_v8_global_v = is_v8_global<T>::value;


template<class T, class = void>
struct is_v8_type : public std::false_type {};

template<class T>
struct is_v8_type<T, std::enable_if_t<
    is_v8_local_v<T> ||
    is_v8_global_v<T> ||
    std::is_same_v<T, v8::Isolate *>
    >> : public std::true_type {};

template<class T>
constexpr bool is_v8_type_v = is_v8_type<T>::value;


} // end namespace v8toolkit

