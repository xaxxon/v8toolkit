
#pragma once

#include <type_traits>
#include <tuple>

#include <v8.h>

#include <xl/demangle.h>

#include "v8helpers.h"
#include "casts.h"

namespace v8toolkit {


template <class T, class = void>
struct cast_to_native_no_value {
    std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int i) const {
        std::cerr << fmt::format("") << std::endl;
        throw InvalidCallException(fmt::format("Not enough javascript parameters for function call - "
                                                   "requires {} but only {} were specified, missing {}",
                                               i+1, info.Length(), xl::demangle<T>()));
    }
};


// if the CastToNative is callable with only an isolate, no v8::Value
template <class T>
struct cast_to_native_no_value<T, std::enable_if_t<std::result_of_t<CastToNative<T>()>::value>> {

    T && operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int i) const {
        return CastToNative<T>()();
    }
};


template<class T, int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
T * get_default_parameter(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
                          DefaultArgsTuple && default_args_tuple) {

    // prioritize the default_args_tuple value if available
    if constexpr(default_arg_position >= 0) {
        return const_cast<T*>(&std::get<default_arg_position>(default_args_tuple));
    } else if constexpr(cast_to_native_supports_default_value_v<T>) {
        stuff.push_back(Stuff<T>::stuffer(CastToNative<T>()(info.GetIsolate())));
        return static_cast<Stuff<T>&>(stuff.back()).get();
    } else {
        throw CastException("No default value available for type {}", xl::demangle<T>());
    }
};

//
//// Helper function for when a required parameter isn't specified in javascript but may have a "global" default value for the type
//template <int default_arg_position = -1, class NoRefT, class DefaultArgsTuple = std::tuple<>>
//NoRefT & set_unspecified_parameter_value(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
//                                DefaultArgsTuple && default_args_tuple)
//{
//    throw CastException("No function-declared default value or 'global' default value for type: {}", xl::demangle<NoRefT>());
//}
//
//
//// Helper function for when a required parameter isn't specified in javascript but may have a "global" default value for the type
//template <int default_arg_position = -1, class NoRefT, class DefaultArgsTuple = std::tuple<>>
//std::enable_if_t<default_arg_position < 0,
//    std::result_of_t<cast_to_native_no_value<NoRefT>(const v8::FunctionCallbackInfo<v8::Value> &, int)> &>
//
//set_unspecified_parameter_value(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
//                                DefaultArgsTuple && default_args_tuple) {
//
//    // for string types, the result returned may be a unique_ptr in order to save memory for it, this is that type
//    using ResultT = std::remove_reference_t<decltype(cast_to_native_no_value<NoRefT>()(info, i++))>;
//    stuff.emplace_back(std::make_unique<Stuff<ResultT>>(std::move(cast_to_native_no_value<NoRefT>()(info, i++))));
//
//    // stuff.back() returns a unique_ptr<StuffBase>
//    return *((static_cast<Stuff<ResultT> *>(stuff.back().get()))->get());
//}
//
//
//// Helper function for when a required parameter isn't specified in javascript but has a function-specific default value specified for it
//template <int default_arg_position = -1, class NoRefT, class DefaultArgsTuple = std::tuple<>>
//
//std::enable_if_t<
//    (default_arg_position >= 0),
//std::result_of_t<cast_to_native_no_value<NoRefT>(const v8::FunctionCallbackInfo<v8::Value> &, int)> &>
//
//set_unspecified_parameter_value(const v8::FunctionCallbackInfo<v8::Value> & info,
//                                int & i,
//                                std::vector<std::unique_ptr<StuffBase>> & stuff,
//                                DefaultArgsTuple && default_args_tuple) {
//
//    // Gets the type returned by CastToNative - it's not always the same type as requested
//    using ResultT = std::remove_reference_t<decltype(cast_to_native_no_value<NoRefT>()(info, i++))>;
//
//    // This should be genealized to not be hardcoded as to what types have a different CastToNative return type
//    if constexpr(std::is_same_v<NoRefT, char*> || std::is_same_v<NoRefT, char const *>) {
//        // get the value out of the default value tuple
//
//
//        char const * string = std::get<(std::size_t) default_arg_position>(std::move(default_args_tuple));
//        int length = strlen(string);
//        std::unique_ptr<char[]> unique_ptr(new char[length + 1]);
//        strcpy(unique_ptr.get(), string);
//
//        stuff.emplace_back(
//            std::make_unique<Stuff < ResultT>>(std::unique_ptr<char[]>(std::move(unique_ptr))));
//    } else {
//        static_assert(std::is_same_v<ResultT, std::remove_reference_t<NoRefT>>,
//                      "Unexpected (i.e. not char*) situation where CastToNative doesn't return same type as requested ");
//        // get the value out of the default value tuple
//        stuff.emplace_back(
//            std::make_unique<Stuff<ResultT>>
//                (std::get<(std::size_t) default_arg_position>(std::move(default_args_tuple)))
//        );
//    }
//
//    return *static_cast<Stuff<ResultT> *>(stuff.back().get())->get();
//
////    // converting the value to a javascript value and then back is an ugly hack to deal with some parameter types changing
////    // types when being returned in order to store the memory somewhere it will be cleaned up (like char*)
////    auto temporary_js_value = v8toolkit::CastToJS<NoRefT>()(info.GetIsolate(), std::get<(std::size_t)default_arg_position>(std::move(default_args_tuple)));
////    stuff.emplace_back(std::make_unique<Stuff<ResultT>>(CastToNative<NoRefT>()(info.GetIsolate(), temporary_js_value)));
////
////    return *((static_cast<Stuff<ResultT> *>(stuff.back().get()))->get());
//}

} // v8toolkit