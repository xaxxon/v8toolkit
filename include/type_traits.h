
#pragma once

#include <type_traits>
#include "wrapped_class_base.h"

namespace v8toolkit {

// always returns false, but can be used to make something dependent
template<class T>
struct always_false : public std::false_type {
};

template<class T> constexpr bool always_false_v = always_false<T>::value;


template<class T>
using void_t = void;

template<class T>
using int_t = int;




template<class T, class = void>
struct is_wrapped_type : public std::false_type {};

template<class T>
struct is_wrapped_type<T, std::enable_if_t<std::is_base_of<v8toolkit::WrappedClassBase, T>::value>> : public std::true_type {};


template<class T>
constexpr bool is_wrapped_type_v = is_wrapped_type<T>::value;


} // end namespace v8toolkit

