
#pragma once

#include <type_traits>
#include <string_view>

#include "wrapped_class_base.h"
#include "stdfunctionreplacement.h"
namespace v8toolkit {

/**
 * always returns false type trait
 */
template<class T>
struct always_false : public std::false_type {
};

template<class T> constexpr bool always_false_v = always_false<T>::value;


/**
 * Whether the specified type is a wrapped type or not.  Specializations may need to be added
 * if additional ways of specifying a wrapped type are added
 */
template<class T, class = void>
struct is_wrapped_type : public std::false_type {};

template<class T>
struct is_wrapped_type<T, std::enable_if_t<std::is_base_of<v8toolkit::WrappedClassBase, T>::value>> : public std::true_type {};

template<class T>
constexpr bool is_wrapped_type_v = is_wrapped_type<T>::value;


/**
 * Is T a specialization of std::basic_string_view?
 */
template<class T>
struct is_string_view : public std::false_type {};

template<class CharT, class Traits>
struct is_string_view<std::basic_string_view<CharT, Traits>> : public std::true_type {};

template<class T>
constexpr bool is_string_view_v = is_string_view<T>::value;


/**
 * is T a type that refers to a string but is not responsible for the memory associated with the string
 */
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


/**
 * Is T is a v8::Local instantiation
 */
template<class T>
struct is_v8_local : public std::false_type {};

template<class T>
struct is_v8_local<v8::Local<T>> : public std::true_type {};

template<class T>
constexpr bool is_v8_local_v = is_v8_local<T>::value;


/**
 * Is T a v8::Global instantiation
 */
template<class T>
struct is_v8_global : public std::false_type {};

template<class T>
struct is_v8_global<v8::Global<T>> : public std::true_type {};

template<class T>
constexpr bool is_v8_global_v = is_v8_global<T>::value;


/**
 * Is T a v8:: type?
 */
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


/**
 * Provides the func::function type which can hold the given 'callable' type
 */
template<class T, class = void>
struct function_type {
    static_assert(always_false_v<T>, "unknown type for std_function_type");
};

// class member functions
template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...)> {
    using type = func::function<R(Args...)>;
};

template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...) const> {
    using type = func::function<R(Args...)>;
};

template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...) &> {
    using type = func::function<R(Args...)>;
};

template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...) const &> {
    using type = func::function<R(Args...)>;
};

template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...) &&> {
    using type = func::function<R(Args...)>;
};

template<class R, class T, class... Args>
struct function_type<R(T::*)(Args...) const && > {
    using type = func::function<R(Args...)>;
};

// free function
template<class R, class... Args>
struct function_type<R(*)(Args...)> {
using type = func::function<R(Args...)>;
};

// std::function
template<class R, class... Args>
struct function_type<std::function<R(Args...)>> {
using type = func::function<R(Args...)>;
};

// func::function
template<class R, class... Args>
struct function_type<func::function<R(Args...)>> {
    using type = func::function<R(Args...)>;
};

// functors
template<class T>
struct function_type<T> {
    using type = typename function_type<decltype(&T::operator())>::type;
};

template<class R, class... Args>
struct function_type<R(Args...)> {
    using type = function_type<R(Args...)>;
};


template<class T>
using function_type_t = typename function_type<T>::type;


/**
 * Returns the std::index_sequence_for for the given func::function type
 */
template<class R, class... Args>
auto get_index_sequence_for_func_function(func::function<R(Args...)>) {
    return std::index_sequence_for<Args...>{};
};


} // end namespace v8toolkit

