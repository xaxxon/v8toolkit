#ifndef CASTS_HPP
#define CASTS_HPP

#include <assert.h>

#include <type_traits>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <deque>
#include <array>
#include <memory>
#include <utility>
#include <set>
#include "v8.h"
#include "wrapped_class_base.h"
#include "v8helpers.h"

namespace v8toolkit {


// always returns false, but can be used to make something dependent
template<class T>
struct always_false : public std::false_type {};

template <class T>  constexpr bool always_false_v = always_false<T>::value;


template<class T, class = void>
struct is_wrapped_type : public std::false_type {};

template<class T>
struct is_wrapped_type<T, std::enable_if_t<std::is_base_of<v8toolkit::WrappedClassBase, T>::value>> : public std::true_type {};


template<class T>
constexpr bool is_wrapped_type_v = is_wrapped_type<T>::value;

// if it's any compination of a reference to a pointer to a wrapped type
template<class T>
constexpr bool is_wrapped_typeish_v =
    is_wrapped_type<
        std::remove_pointer_t<
            std::remove_reference_t<T>
        >
    >::value;


    // if a value to send to a macro has a comma in it, use this instead so it is parsed as a comma character in the value
    //   and not separating another parameter to the template
#define V8TOOLKIT_COMMA ,


    // add inside CastToNative::operator() to have it handle
    //   with no parameters
#define HANDLE_FUNCTION_VALUES \
    { \
	if (value->IsFunction()) { \
	    value = v8toolkit::call_simple_javascript_function(isolate, v8::Local<v8::Function>::Cast(value)); \
	} \
    }



#define CAST_TO_NATIVE(TYPE, FUNCTION_BODY) \
template<> \
 struct v8toolkit::CastToNative<TYPE> {				\
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const FUNCTION_BODY \
        static constexpr bool callable(){return true;} /* It wouldn't be selected if it weren't callable */ \
};




#define CAST_TO_JS(TYPE, FUNCTION_BODY) 					\
template<> \
 struct v8toolkit::CastToJS<TYPE> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const & value) const FUNCTION_BODY \
};

/**
* Casts from a boxed Javascript type to a native type
*/
template<typename T, class = void>
struct CastToNative {
    template<class U = T> // just to make it dependent so the static_asserts don't fire before `callable` can be called
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        static_assert(!std::is_pointer<T>::value, "Cannot CastToNative to a pointer type of an unwrapped type");
        static_assert(!(std::is_lvalue_reference<T>::value && !std::is_const<std::remove_reference_t<T>>::value),
                      "Cannot CastToNative to a non-const "
                          "lvalue reference of an unwrapped type because there is no lvalue variable to send");
        static_assert(!is_wrapped_type_v<T>,
                      "CastToNative<SomeWrappedType> shouldn't fall through to this specialization");
        static_assert(always_false_v<T>, "Invalid CastToNative configuration");
    }
    static constexpr bool callable(){return false;}
};


//template<class T>
//struct is_wrapped_type<T, std::enable_if_t<std::is_reference<
//    typename std::result_of_t<
//        CastToNative<
//            std::remove_pointer_t<
//                std::remove_reference_t<T>
//            >                                 /* remove_pointer */
//        >(v8::Isolate*, v8::Local<v8::Value>) /* CastToNative */
//    >                                         /* result_of */
//>::value>>  : public std::true_type {};



template<>
struct CastToNative<void> {
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {}
};


CAST_TO_NATIVE(bool, {return static_cast<bool>(value->ToBoolean()->Value());});



// integers
CAST_TO_NATIVE(long long, {return static_cast<long long>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned long long, {return static_cast<unsigned long long>(value->ToInteger()->Value());});

CAST_TO_NATIVE(long, {return static_cast<long>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned long, {return static_cast<unsigned long>(value->ToInteger()->Value());});

CAST_TO_NATIVE(int, { return static_cast<int>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned int, {return static_cast<unsigned int>(value->ToInteger()->Value());});

CAST_TO_NATIVE(short, {return static_cast<short>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned short, {return static_cast<unsigned short>(value->ToInteger()->Value());});

CAST_TO_NATIVE(char, {return static_cast<char>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned char, {return static_cast<unsigned char>(value->ToInteger()->Value());});

CAST_TO_NATIVE(wchar_t, {return static_cast<wchar_t>(value->ToInteger()->Value());});
CAST_TO_NATIVE(char16_t, {return static_cast<char16_t>(value->ToInteger()->Value());});

CAST_TO_NATIVE(char32_t, {return static_cast<char32_t>(value->ToInteger()->Value());});




template<class... Ts, std::size_t... Is>
std::tuple<Ts...> cast_to_native_tuple_helper(v8::Isolate *isolate, v8::Local<v8::Array> array, std::tuple<Ts...>, std::index_sequence<Is...>) {
    return std::tuple<Ts...>(CastToNative<Ts>()(isolate, array->Get(Is))...);
}

template<class... Ts>
struct CastToNative<std::tuple<Ts...>>
{
    std::tuple<Ts...> operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        if (!value->IsArray()) {
            throw v8toolkit::CastException(fmt::format("CastToNative tried to create a {} object but was not given a JavaScript array", demangle<std::tuple<Ts...>>()));
        }
        v8::Local<v8::Array> array = v8::Local<v8::Array>::Cast(value);

        return cast_to_native_tuple_helper(isolate, array, std::tuple<Ts...>(), std::index_sequence_for<Ts...>());
    }
};

// If the type returns an rvalue, then the the const version is the same as the non-const version
template<class T>
struct CastToNative<T const,
    std::enable_if_t<!is_wrapped_type_v<T>>> {
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return CastToNative<T>()(isolate, value);
    }
};


// A T const & can take an rvalue, so send it one, since an actual object isn't available for non-wrapped types
template<class T>
struct CastToNative<T const &, std::enable_if_t<!is_wrapped_type_v<T>>> {
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return CastToNative<T const>()(isolate, value);
    }
};

// A T && can take an rvalue, so send it one, since a previously existing object isn't available for non-wrapped types
template<class T>
struct CastToNative<T &&, std::enable_if_t<!is_wrapped_type_v<T>>> {
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return CastToNative<T const>()(isolate, value);
    }
};



template<template<class,class> class ContainerTemplate, class FirstT, class SecondT>
ContainerTemplate<FirstT, SecondT> pair_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {
    HANDLE_FUNCTION_VALUES;
    if (value->IsArray()) {
        auto length = get_array_length(isolate, value);
        if (length != 2) {
            auto error = fmt::format("Array to std::pair must be length 2, but was {}", length);
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, error.c_str()));
            throw v8toolkit::CastException(error);
        }
        auto context = isolate->GetCurrentContext();
        auto array = get_value_as<v8::Array>(value);
        auto first = array->Get(context, 0).ToLocalChecked();
        auto second = array->Get(context, 1).ToLocalChecked();
        return std::pair<FirstT, SecondT>(v8toolkit::CastToNative<FirstT>()(isolate, first),
                                          v8toolkit::CastToNative<SecondT>()(isolate, second));

    } else {
        auto error = fmt::format("CastToNative<std::pair<T>> requires an array but instead got %s\n", stringify_value(isolate, value));
        std::cout << error << std::endl;
        throw v8toolkit::CastException(error);
    }
}


template<class FirstT, class SecondT>
struct v8toolkit::CastToNative<std::pair<FirstT, SecondT>>{
    std::pair<FirstT, SecondT> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return pair_type_helper<std::pair, FirstT, SecondT>(isolate, value);
    }
};


CAST_TO_NATIVE(float, { return static_cast<float>(value->ToNumber()->Value());});
CAST_TO_NATIVE(double, { return static_cast<double>(value->ToNumber()->Value());});
CAST_TO_NATIVE(long double, { return static_cast<long double>(value->ToNumber()->Value());});



template<>
struct CastToNative<v8::Local<v8::Function>> {
	v8::Local<v8::Function> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        if(value->IsFunction()) {
            return v8::Local<v8::Function>::Cast(value);
        } else {
            throw CastException(fmt::format(
                    "CastToNative<v8::Local<v8::Function>> requires a javascript function but instead got '{}'",
                    stringify_value(isolate, value)));
        }
    }
};

/**
 * char * and const char * are the only types that don't actually return their own type.  Since a buffer is needed
 *   to store the string, a std::unique_ptr<char[]> is returned.
 */
template<>
struct CastToNative<char *> {
  std::unique_ptr<char[]> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
      HANDLE_FUNCTION_VALUES;
    return std::unique_ptr<char[]>(strdup(*v8::String::Utf8Value(value)));
  }
};
template<>
struct CastToNative<const char *> {
  std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
      HANDLE_FUNCTION_VALUES;
      return CastToNative<char *>()(isolate, value);
  }
};

CAST_TO_NATIVE(std::string, {
    return std::string(*v8::String::Utf8Value(value));
});


template<template<class,class...> class VectorTemplate, class T, class... Rest>
auto vector_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) ->
    VectorTemplate<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>
{
    using ValueType = std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>;
    static_assert(!std::is_reference<ValueType>::value, "vector-like value type cannot be reference");
    using ResultType = VectorTemplate<ValueType, Rest...>;
    HANDLE_FUNCTION_VALUES;
    auto context = isolate->GetCurrentContext();
    ResultType v;
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for (int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            v.emplace_back(std::forward<T>(CastToNative<T>()(isolate, value)));
        }
    } else {
        throw CastException(fmt::format("CastToNative<std::vector-like<{}>> requires an array but instead got JS: '{}'",
                                        demangle<T>(),
                                        stringify_value(isolate, value)));
    }
    return v;
}


template<template<class,class...> class SetTemplate, class T, class... Rest>
auto set_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) ->
SetTemplate<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>
{
    using ValueType = std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>;
    static_assert(!std::is_reference<ValueType>::value, "Set-like value type cannot be reference");
    using ResultType = SetTemplate<ValueType, Rest...>;
    HANDLE_FUNCTION_VALUES;
    auto context = isolate->GetCurrentContext();
    ResultType set;
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for (int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            set.emplace(std::forward<T>(CastToNative<T>()(isolate, value)));
        }
    } else {
        throw CastException(fmt::format("CastToNative<std::vector-like<{}>> requires an array but instead got JS: '{}'",
                                        demangle<T>(),
                                        stringify_value(isolate, value)));
    }
    return set;
}



//Returns a vector of the requested type unless CastToNative on ElementType returns a different type, such as for char*, const char *
// Must make copies of all the values
template<class T, class... Rest>
struct CastToNative<std::vector<T, Rest...>, std::enable_if_t<std::is_copy_constructible<T>::value>> {
    using ResultType = std::vector<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

    ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        return vector_type_helper<std::vector, T, Rest...>(isolate, value);
    }
};

// can move the elements if the underlying JS objects own their memory or can do copies if copyable, othewrise throws
// SFINAE on this is required for disambiguation, even though it can't ever catch anything
template<class T, class... Rest>
struct CastToNative<std::vector<T, Rest...> &&, std::enable_if_t<!is_wrapped_type_v<std::vector<T, Rest...>>>> {
    using ResultType = std::vector<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

    ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        return vector_type_helper<std::vector, std::add_rvalue_reference_t<T>, Rest...>(isolate, value);
    }
};


template<class T, class... Rest>
struct CastToNative<std::set<T, Rest...>, std::enable_if_t<!is_wrapped_type_v<std::set<T, Rest...>>>> {
    using ResultType = std::set<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

    ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        return set_type_helper<std::set, std::add_rvalue_reference_t<T>, Rest...>(isolate, value);
    }
};







// Cast a copyable, standard type to a unique_ptr
template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>,
    std::enable_if_t<
        std::is_copy_constructible<T>::value &&
        !is_wrapped_type_v<T>
    >// end enable_if_t
>// end template
{
    std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return std::unique_ptr<T, Rest...>(new T(CastToNative<T>()(isolate, value)));
    }
};


template<class T>
struct CastToNative<v8::Local<T>> {
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> value) const {
        return value;
    }
};

// cannot cast a non-copyable, standard type to a unique_ptr
template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<!std::is_copy_constructible<T>::value && !is_wrapped_type_v<T>>>;  // INTENTIONALLY NOT IMPLEMENTED

template<template<class,class,class...> class ContainerTemplate, class Key, class Value, class... Rest>
ContainerTemplate<Key, Value, Rest...> map_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    //    MapType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    if (!value->IsObject()) {
        throw CastException(
                fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                            stringify_value(isolate, value)));
    }

    auto context = isolate->GetCurrentContext();

    ContainerTemplate<Key, Value, Rest...> results;
    for_each_own_property(context, value->ToObject(),
                          [isolate, &results](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              results.emplace(v8toolkit::CastToNative<Key>()(isolate, key),
                                              v8toolkit::CastToNative<Value>()(isolate, value));
                          });
    return results;
}

template<class Key, class Value, class... Args>
struct CastToNative<std::map<Key, Value, Args...>> {
    std::map<Key, Value, Args...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return map_type_helper<std::map, Key, Value, Args...>(isolate, value);
    }
};

template<template<class,class,class...> class ContainerTemplate, class Key, class Value, class... Rest>
ContainerTemplate<Key, Value, Rest...> multimap_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    if (!value->IsObject()) {
        throw CastException(
                fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                            stringify_value(isolate, value)));
    }

    auto context = isolate->GetCurrentContext();

    ContainerTemplate<Key, Value, Rest...> results;
    for_each_own_property(context, value->ToObject(),
                          [&](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              v8toolkit::for_each_value(context, value, [&](v8::Local<v8::Value> sub_value){
                                  results.emplace(v8toolkit::CastToNative<Key>()(isolate, key),
                                                  v8toolkit::CastToNative<Value>()(isolate, sub_value));
                              });
                          });
    return results;
}



template<class Key, class Value, class... Args>
struct CastToNative<std::multimap<Key, Value, Args...>> {

    using ResultType = std::multimap<Key, Value, Args...>;

    ResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return multimap_type_helper<std::multimap, Key, Value, Args...>(isolate, value);
    }
};


//


/**
* Casts from a native type to a boxed Javascript type
*/

template<typename T, class = void>
struct CastToJS {
    static_assert(always_false_v<T>, "Fallback CastToJS template isn't allowed");
};


template<class T>
struct CastToJS<T &, std::enable_if_t<!is_wrapped_type_v<T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & value) const {
        return CastToJS<T>()(isolate, value);
    }
};


template<class T>
struct CastToJS<T *, std::enable_if_t<!is_wrapped_type_v<T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const value) const {
        if (value == nullptr) {
            return v8::Undefined(isolate);
        } else {
            return CastToJS<T>()(isolate, *value);
        }
    }
};



/**
 * For non-wrapped types, the result of casting to a const type is the same as casting to the non-const type.  A copy.
 * @tparam T
 */
template<class T>
struct CastToJS<T const,
    std::enable_if_t<!is_wrapped_type_v<T>> // enable_if
> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const & value) const {
        return v8toolkit::CastToJS<T>()(isolate, value);
    }
};




CAST_TO_JS(bool, {return v8::Boolean::New(isolate, value);});

//TODO: Should all these operator()'s be const?
// integers
CAST_TO_JS(char, {return v8::Integer::New(isolate, value);});
CAST_TO_JS(unsigned char, {return v8::Integer::New(isolate, value);});

CAST_TO_JS(wchar_t, {return v8::Number::New(isolate, value);});
CAST_TO_JS(char16_t, {return v8::Integer::New(isolate, value);});
CAST_TO_JS(char32_t, {return v8::Integer::New(isolate, value);});
CAST_TO_JS(short, {return v8::Integer::New(isolate, value);});
CAST_TO_JS(unsigned short, {return v8::Integer::New(isolate, value);});



CAST_TO_JS(int, {return v8::Number::New(isolate, value);});

CAST_TO_JS(unsigned int, {return v8::Number::New(isolate, value);});
CAST_TO_JS(long, {return v8::Number::New(isolate, value);});

CAST_TO_JS(unsigned long, {return v8::Number::New(isolate, value);});
CAST_TO_JS(long long, {return v8::Number::New(isolate, static_cast<double>(value));});
CAST_TO_JS(unsigned long long, {return v8::Number::New(isolate, static_cast<double>(value));});



// floats
CAST_TO_JS(float, {return v8::Number::New(isolate, value);});
CAST_TO_JS(double, {return v8::Number::New(isolate, value);});
CAST_TO_JS(long double, {return v8::Number::New(isolate, value);});


CAST_TO_JS(std::string, {return v8::String::NewFromUtf8(isolate, value.c_str(), v8::String::kNormalString, value.length());});

CAST_TO_JS(char *, {return v8::String::NewFromUtf8(isolate, value);});
CAST_TO_JS(char const *, {return v8::String::NewFromUtf8(isolate, value);});

template<class T>
struct CastToJS<T**> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const T** multi_pointer) {
        return CastToJS<T*>(isolate, *multi_pointer);
    }
};


template<class R, class... Params>
struct CastToJS<std::function<R(Params...)>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::function<R(Params...)> & function) {
        return v8::String::NewFromUtf8(isolate, "CastToJS of std::function not supported yet");
    }
};



/**
* Special passthrough type for objects that want to take javascript object objects directly
*/
template<class T>
struct CastToJS<v8::Local<T>> {
	v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> value){
		//return v8::Local<v8::Value>::New(isolate, object);
        return value;
	}
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Global<T> && value){
        //return v8::Local<v8::Value>::New(isolate, object);
        return value.Get(isolate);
    }
};




template<class T>
struct CastToJS<v8::Global<T>> {
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> & value) {
        value;
    }
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Global<T> const & value) {
        return value.Get(isolate);
    }
};




// CastToJS<std::pair<>>
template<class T1, class T2>
struct CastToJS<std::pair<T1, T2>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::pair<T1, T2> const & pair);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::pair<T1, T2> && pair) {
        return this->operator()(isolate, pair);
    }
};


template<template<class, class, class...> class MapTemplate, class KeyType, class ValueType, class ReferenceTypeIndicator, class... Rest>
v8::Local<v8::Value> cast_to_js_map_helper(v8::Isolate * isolate, MapTemplate<KeyType, ValueType, Rest...> const & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);

    using KeyForwardT = std::conditional_t<std::is_rvalue_reference_v<ReferenceTypeIndicator>, std::add_rvalue_reference_t<KeyType>, std::add_lvalue_reference_t<KeyType>>;
    using ValueForwardT = std::conditional_t<std::is_rvalue_reference_v<ReferenceTypeIndicator>, std::add_rvalue_reference_t<ValueType>, std::add_lvalue_reference_t<ValueType>>;


    for(auto & pair : map) {
        (void) object->Set(context,
                           CastToJS<std::remove_reference_t<KeyType>>  ()(isolate, std::forward<KeyForwardT>  (const_cast<KeyForwardT>  (pair.first ))),
                           CastToJS<std::remove_reference_t<ValueType>>()(isolate, std::forward<ValueForwardT>(const_cast<ValueForwardT>(pair.second)))
        );
    }
    return object;
}


template<template<class...> class VectorTemplate, class ValueType, class... Rest>
v8::Local<v8::Value> cast_to_js_vector_helper(v8::Isolate * isolate, VectorTemplate<std::remove_reference_t<ValueType>, Rest...> const & vector) {

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);

    int i = 0;
    for (auto & element : vector) {
        (void) array->Set(context, i, CastToJS<std::remove_reference_t<ValueType>>()(isolate, std::forward<ValueType>(const_cast<ValueType>(element))));
        i++;
    }
    return array;
}


// CastToJS<std::vector<>>
template<class T, class... Rest>
struct CastToJS<std::vector<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::vector<T, Rest...> const & vector);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::vector<T, Rest...> && vector);
};


// CastToJS<std::list>
template<class U, class... Rest>
struct CastToJS<std::list<U, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::list<U, Rest...> & list);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::list<U, Rest...> && list) {
        return this->operator()(isolate, list);
    }
};

// CastToJS<std::map>
template<class KeyType, class ValueType, class... Rest>
struct CastToJS<std::map<KeyType, ValueType, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::map<KeyType, ValueType, Rest...> const & map)  {
        return cast_to_js_map_helper<std::map, KeyType, ValueType, int&, Rest...>(isolate, map);
    }

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::map<KeyType, ValueType, Rest...> && map)  {
        return cast_to_js_map_helper<std::map, KeyType, ValueType, int&&, Rest...>(isolate, map);
    }

};

// CastToJS<std::multimap>
template<class A, class B, class... Rest>
struct CastToJS<std::multimap<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::multimap<A, B, Rest...> & multimap);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::multimap<A, B, Rest...> && multimap) {
        return this->operator()(isolate, multimap);
    }
};


// CastToJS<std::undordered:map>
template<class A, class B, class... Rest>
struct CastToJS<std::unordered_map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unordered_map<A, B, Rest...> & unorderedmap);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unordered_map<A, B, Rest...> && unorderedmap) {
        return this->operator()(isolate, unorderedmap);
    }
};

// CastToJS<std::deque>
template<class T, class... Rest>
struct CastToJS<std::deque<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::deque<T, Rest...> & deque);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::deque<T, Rest...> && deque) {
        return this->operator()(isolate, deque);
    }
};

// CastToJS<std::array>
template<class T, std::size_t N>
struct CastToJS<std::array<T, N>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::array<T, N> & arr);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::array<T, N> && arr) {
        return this->operator()(isolate, arr);
    }
};



/**
 * This is for when a function returns a std::unique - meaning it likely allocated new memory on its own
 * If this is being sent back to JS, the unique_ptr must release the memory, because the unique_ptr is going to
 * go out of scope immediately
 *
 * These functions are not const because they call unique_ptr::release
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...>, std::enable_if_t<!is_wrapped_type_v<T>>> {

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> & unique_ptr) {
        return CastToJS<T>()(isolate, *unique_ptr.get());
    }


    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> && unique_ptr) {
        auto result = CastToJS<T>()(isolate, std::move(*unique_ptr));
        unique_ptr.reset();
        return result;
    }
};


/**
 * If a data structure contains a unique_ptr and that is being returned, the unique_ptr should not ::release()
 * its memory.  This is treated just as if the call were returning a T* instead of a unique_ptr<T>
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> &, std::enable_if_t<!is_wrapped_type_v<std::unique_ptr<T, Rest...>>>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> const & unique_ptr) {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());
        if (unique_ptr.get() == nullptr) {
            return v8::Undefined(isolate);
        } else {
            return CastToJS<T*>()(isolate, unique_ptr.get());
        }
    }
};
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> const &, std::enable_if_t<!is_wrapped_type_v<std::unique_ptr<T, Rest...> const>>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> const & unique_ptr)  {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());

        if (unique_ptr.get() == nullptr) {
            return v8::Undefined(isolate);
        } else {
            return CastToJS<T*>()(isolate, unique_ptr.get());
        }
    }
};



template<class T>
struct CastToJS<std::shared_ptr<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::shared_ptr<T> const & shared_ptr) {
        return CastToJS<T>()(isolate, *shared_ptr.get());
    }
};




template<class T, class... Rest>
v8::Local<v8::Value> CastToJS<std::vector<T, Rest...>>::operator()(v8::Isolate *isolate, std::vector<T, Rest...> const & vector) {
    return cast_to_js_vector_helper<std::vector, T&, Rest...>(isolate, vector);
}

template<class T, class... Rest>
v8::Local<v8::Value> CastToJS<std::vector<T, Rest...>>::operator()(v8::Isolate *isolate, std::vector<T, Rest...> && vector) {
    return cast_to_js_vector_helper<std::vector, T&&, Rest...>(isolate, vector);
}





/**
* supports lists containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::list<T, Rest...>>::operator()(v8::Isolate * isolate, std::list<T, Rest...> & list) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    int i = 0;
    for (auto & element : list) {
        (void)array->Set(context, i, CastToJS<T>()(isolate, element));
        i++;
    }
    return array;
}




template<template<class,class,class...> class MultiMapLike, class A, class B, class... Rest>
v8::Local<v8::Object> casttojs_multimaplike(v8::Isolate * isolate, MultiMapLike<A, B, Rest...> const & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto & pair : map){
        auto key = CastToJS<A>()(isolate, const_cast<A&>(pair.first));
        auto value = CastToJS<B>()(isolate, const_cast<B&>(pair.second));

        // check to see if a value with this key has already been added
        bool default_value = true;
        bool object_has_key = object->Has(context, key).FromMaybe(default_value);
        if(!object_has_key) {
            // get the existing array, add this value to the end
            auto array = v8::Array::New(isolate);
            (void)array->Set(context, 0, value);
            (void)object->Set(context, key, array);
        } else {
            // create an array, add the current value to it, then add it to the object
            auto existing_array_value = object->Get(context, key).ToLocalChecked();
            v8::Handle<v8::Array> existing_array = v8::Handle<v8::Array>::Cast(existing_array_value);

            //find next array position to insert into (is there no better way to push onto the end of an array?)
            int i = 0;
            while(existing_array->Has(context, i).FromMaybe(default_value)){i++;}
            (void)existing_array->Set(context, i, value);
        }
    }
    return object;
}

/**
* supports maps containing any type also supported by CastToJS to javascript arrays
* It creates an object of key => [values...]
* All values are arrays, even if there is only one value in the array.
*/
template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::multimap<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> & map){
    return casttojs_multimaplike(isolate, map);
}






template<class T1, class T2> v8::Local<v8::Value>
CastToJS<std::pair<T1, T2>>::operator()(v8::Isolate *isolate, std::pair<T1, T2> const & pair) {

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    (void) array->Set(context, 0, CastToJS<T1 &>()(isolate, pair.first));
    (void) array->Set(context, 1, CastToJS<T2 &>()(isolate, pair.second));
    return array;
}




template<int position, class T>
struct CastTupleToJS;

template<class... Args>
struct CastTupleToJS<0, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - 0 - 1;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        using TuplePositionType = typename std::tuple_element<array_position, std::tuple<Args...>>::type;

        (void)array->Set(context,
                         array_position,
                         CastToJS<TuplePositionType>()(isolate,
                                                       std::get<array_position>(tuple)));
        return array;
    }
};

template<int position, class... Args>
struct CastTupleToJS<position, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - position - 1;
        using TuplePositionType = typename std::tuple_element<array_position, std::tuple<Args...>>::type;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = CastTupleToJS<position - 1, std::tuple<Args...>>()(isolate, tuple);
        (void)array->Set(context,
                         array_position,
                         CastToJS<TuplePositionType>()(isolate,
                                                       std::get<array_position>(tuple)));
        return array;
    }
};



template<class... Args>
struct CastToJS<std::tuple<Args...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::tuple<Args...> tuple) {
        return CastTupleToJS<sizeof...(Args) - 1, std::tuple<Args...>>()(isolate, tuple);
    }
};


/**
* supports unordered_maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::unordered_map<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> & map){
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto pair : map){
        (void)object->Set(context, CastToJS<A&>()(isolate, pair.first), CastToJS<B&>()(isolate, pair.second));
    }
    return object;
}




/**
* supports deques containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::deque<T, Rest...>>::operator()(v8::Isolate * isolate, std::deque<T, Rest...> & deque) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    auto size = deque.size();
    for(unsigned int i = 0; i < size; i++) {
        (void)array->Set(context, i, CastToJS<T>()(isolate, deque.at(i)));
    }
    return array;
}



    template<class T, std::size_t N> v8::Local<v8::Value>
CastToJS<std::array<T, N>>::operator()(v8::Isolate * isolate, std::array<T, N> & arr) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    // auto size = arr.size();
    for(unsigned int i = 0; i < N; i++) {
        (void)array->Set(context, i, CastToJS<T>()(isolate, arr[i]));
    }
    return array;
}



//TODO: forward_list

//TODO: stack



template<class T, class... Rest>
struct CastToJS<std::set<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::set<T, Rest...> const & set) {
        return cast_to_js_vector_helper<std::set, T&, Rest...>(isolate, set);
    }
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::set<T, Rest...> && set) {
        return cast_to_js_vector_helper<std::set, T&&, Rest...>(isolate, set);
    }
};





//TODO: unordered_set




template<class ReturnT, class... Args>
struct CastToNative<std::function<ReturnT(Args...)>> {
    std::function<ReturnT(Args...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const;
//    {
//        if (!value->IsFunction()) {
//            throw CastException("CastToNative<{}> requires a JavaScript function parameter, not {}", demangle<std::function<ReturnT(Args...)>>(), stringify_value(isolate, value));
//        }
//
//        auto function = get_value_as<v8::Function>(value);
//        auto context = isolate->GetCurrentContext();
//        return [&](Args&&... args)->ReturnT{
//
//            auto javascript_function_result = call_javascript_function_with_vars(context,
//                                                                                 function,
//                                                                                 context->Global(),
//                                                                                 TypeList<Args...>(),
//                                                                                 std::forward<Args>(args)...);
//
//            return CastToNative<ReturnT>()(isolate, javascript_function_result);
//        };
//    }
};

} // end namespace v8toolkit


#ifdef V8TOOLKIT_ENABLE_EASTL_SUPPORT
#include "casts_eastl.hpp"
#endif

#endif // CASTS_HPP
