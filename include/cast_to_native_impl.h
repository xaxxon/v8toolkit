
#include "cast_to_native.h"
#include "call_javascript_function.h"
#include "v8helpers.h"

namespace v8toolkit {


/**
* Casts from a boxed Javascript type to a native type
*/
template<typename T, class>
struct CastToNative {
    template<class U = T> // just to make it dependent so the static_asserts don't fire before `callable` can be called
    T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        static_assert(!std::is_pointer<T>::value, "Cannot CastToNative to a pointer type of an unwrapped type");
        static_assert(!(std::is_lvalue_reference<T>::value && !std::is_const<std::remove_reference_t<T>>::value),
                      "Cannot CastToNative to a non-const "
                          "lvalue reference of an unwrapped type because there is no lvalue variable to send");
        static_assert(!is_wrapped_type_v<T>,
                      "CastToNative<SomeWrappedType> shouldn't fall through to this specialization");
        static_assert(always_false_v<T>, "Invalid CastToNative configuration - maybe an unwrapped type without a CastToNative defined for it?");
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
    static constexpr bool callable(){return true;}
};


CAST_TO_NATIVE(bool, {HANDLE_FUNCTION_VALUES; return static_cast<bool>(value->ToBoolean()->Value());});



// integers
CAST_TO_NATIVE(long long, {HANDLE_FUNCTION_VALUES; return static_cast<long long>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned long long, {HANDLE_FUNCTION_VALUES; return static_cast<unsigned long long>(value->ToInteger()->Value());});

CAST_TO_NATIVE(long, {HANDLE_FUNCTION_VALUES; return static_cast<long>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned long, {HANDLE_FUNCTION_VALUES; return static_cast<unsigned long>(value->ToInteger()->Value());});

CAST_TO_NATIVE(int, {HANDLE_FUNCTION_VALUES; return static_cast<int>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned int, {HANDLE_FUNCTION_VALUES; return static_cast<unsigned int>(value->ToInteger()->Value());});

CAST_TO_NATIVE(short, {HANDLE_FUNCTION_VALUES; return static_cast<short>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned short, {HANDLE_FUNCTION_VALUES; return static_cast<unsigned short>(value->ToInteger()->Value());});

CAST_TO_NATIVE(char, {HANDLE_FUNCTION_VALUES; return static_cast<char>(value->ToInteger()->Value());});
CAST_TO_NATIVE(unsigned char, {HANDLE_FUNCTION_VALUES; return static_cast<unsigned char>(value->ToInteger()->Value());});

CAST_TO_NATIVE(wchar_t, {HANDLE_FUNCTION_VALUES; return static_cast<wchar_t>(value->ToInteger()->Value());});
CAST_TO_NATIVE(char16_t, {HANDLE_FUNCTION_VALUES; return static_cast<char16_t>(value->ToInteger()->Value());});

CAST_TO_NATIVE(char32_t, {HANDLE_FUNCTION_VALUES; return static_cast<char32_t>(value->ToInteger()->Value());});




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
static constexpr bool callable(){return true;}


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
static constexpr bool callable(){return true;}

};

// A T && can take an rvalue, so send it one, since a previously existing object isn't available for non-wrapped types
template<class T>
struct CastToNative<T &&, std::enable_if_t<!is_wrapped_type_v<T>>> {
T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    return CastToNative<T const>()(isolate, value);
}
static constexpr bool callable(){return true;}

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


CAST_TO_NATIVE(float, {HANDLE_FUNCTION_VALUES; return static_cast<float>(value->ToNumber()->Value());});
CAST_TO_NATIVE(double, {HANDLE_FUNCTION_VALUES; return static_cast<double>(value->ToNumber()->Value());});
CAST_TO_NATIVE(long double, {HANDLE_FUNCTION_VALUES; return static_cast<long double>(value->ToNumber()->Value());});



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
static constexpr bool callable(){return true;}

};


// Returns a std::unique_ptr<char[]> because a char * doesn't hold it's own memory
template<>
struct CastToNative<char *> {
    std::unique_ptr<char[]> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return std::unique_ptr<char[]>(strdup(*v8::String::Utf8Value(value)));
    }
    static constexpr bool callable(){return true;}

};

// Returns a std::unique_ptr<char[]> because a char const * doesn't hold it's own memory
template<>
struct CastToNative<const char *> {
    std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return CastToNative<char *>()(isolate, value);
    }
    static constexpr bool callable(){return true;}
};

// Returns a std::unique_ptr<char[]> because a string_view doesn't hold it's own memory
template<class CharT, class Traits>
struct CastToNative<std::basic_string_view<CharT, Traits>> {
    std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return CastToNative<char *>()(isolate, value);
    }
    static constexpr bool callable(){return true;}
};



template<class CharT, class Traits, class Allocator>
struct CastToNative<std::basic_string<CharT, Traits, Allocator>> {
    std::string operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        HANDLE_FUNCTION_VALUES;
        return std::string(*v8::String::Utf8Value(value));
    }
    static constexpr bool callable(){return true;}
};



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
    static constexpr bool callable(){return true;}

};

// can move the elements if the underlying JS objects own their memory or can do copies if copyable, othewrise throws
// SFINAE on this is required for disambiguation, even though it can't ever catch anything
template<class T, class... Rest>
struct CastToNative<std::vector<T, Rest...> &&, std::enable_if_t<!is_wrapped_type_v<std::vector<T, Rest...>>>> {
using ResultType = std::vector<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
    return vector_type_helper<std::vector, std::add_rvalue_reference_t<T>, Rest...>(isolate, value);
}
static constexpr bool callable(){return true;}

};


template<class T, class... Rest>
struct CastToNative<std::set<T, Rest...>, std::enable_if_t<!is_wrapped_type_v<std::set<T, Rest...>>>> {
using ResultType = std::set<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
    return set_type_helper<std::set, std::add_rvalue_reference_t<T>, Rest...>(isolate, value);
}
static constexpr bool callable(){return true;}

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
static constexpr bool callable(){return true;}

};


template<class T>
struct CastToNative<v8::Local<T>> {
    v8::Local<T> operator()(v8::Isolate * isolate, v8::Local<T> value) const {
        return value;
    }
    static constexpr bool callable(){return true;}

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
    static constexpr bool callable(){return true;}

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
    static constexpr bool callable(){return true;}

};


//



//TODO: unordered_set




template<class ReturnT, class... Args>
struct CastToNative<std::function<ReturnT(Args...)>> {
    std::function<ReturnT(Args...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        auto js_function = v8toolkit::get_value_as<v8::Function>(value);

        // v8::Global's aren't copyable, but shared pointers to them are. std::functions need everything in them to be copyable
        auto context = isolate->GetCurrentContext();
        auto shared_global_function = std::make_shared<v8::Global<v8::Function>>(isolate, js_function);
        auto shared_global_context = std::make_shared<v8::Global<v8::Context>>(isolate, context);

        return [isolate, shared_global_function, shared_global_context](Args... args) -> ReturnT {
            v8::Locker locker(isolate);
            v8::HandleScope sc(isolate);
            auto context = shared_global_context->Get(isolate);
            return v8toolkit::scoped_run(isolate, context, [&]() -> ReturnT {
                assert(!context.IsEmpty());
                auto result = v8toolkit::call_javascript_function(context,
                                                                  shared_global_function->Get(isolate),
                                                                  context->Global(),
                                                                  std::tuple<Args...>(args...));
                return CastToNative<ReturnT>()(isolate, result);
            });
        };
    }
};



} // end v8toolkit namespace
