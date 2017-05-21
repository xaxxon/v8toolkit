#pragma once

#include <tuple>


#include <v8.h>

#include "casts.hpp"
#include "v8helpers.h"
#include "unspecified_parameter_value.h"

namespace v8toolkit {

// define this to log debugging messages while running
#define V8TOOLKIT_PARAMETER_BUILDER_LOGGING

#ifdef V8TOOLKIT_PARAMETER_BUILDER_LOGGING
#define PB_PRINT(format_string, ...) \
    std::cerr << fmt::format(format_string, ##__VA_ARGS__) << std::endl;
#else
#define PB_PRINT(format_string, ...)
#endif


/**
 * Used by CallCallable to build an actual list of values to send to a function.  Each call to ParameterBuilder
 * is responsible for one parameter.
 *
 * If the parameter isn't self contained (like a char *), then the memory allocated for it will be stored in `stuff`
 * which is automatically cleaned up when the function returns.
 *
 * If no explicit parameter is provided from JavaScript, a default value may be available in `default_args_tuple`
 */
template<class T, class = void>
struct ParameterBuilder;


/**
 * Handles lvalue reference types by delegating the call to the pointer version and dereferencing the
 * returned pointer - except for char type, because char [const] * is special
 */
template <class T>
struct ParameterBuilder<T &, std::enable_if_t<!std::is_same_v<std::remove_const_t<T>, char> && !is_wrapped_type_v<T>>> {
    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                 std::vector<std::unique_ptr<StuffBase>> & stuff,
                 DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling lvalue reference: {}", demangle<T>());
        return * ParameterBuilder<T*>()(info, i, stuff, std::move(default_args_tuple));
    }
};


template <class T>
struct ParameterBuilder<T &, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char> && !is_wrapped_type_v<T>>> {
    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling lvalue reference for char: {}", demangle<T>());
        auto value = info[i++];
        auto isolate = info.GetIsolate();
        stuff.push_back(std::make_unique<Stuff<char>>(std::make_unique<char>(CastToNative<char>()(isolate, value))));
        return *static_cast<Stuff<char> &>(*stuff.back()).get();

    }
};


template <class T>
struct ParameterBuilder<T &&, std::enable_if_t<!is_wrapped_type_v<T>>> {

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T && operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling rvalue reference to unwrapped type: {}", demangle<T>());
        std::cerr << fmt::format("default_arg_position: {}", default_arg_position) << std::endl;
        return std::move(*(ParameterBuilder<T*>().template operator()<default_arg_position>(info, i, stuff, std::move(default_args_tuple))));
    }
};



/**
* Pointers to unwrapped types
*/
template <class T>
struct ParameterBuilder<T*, std::enable_if_t<!is_wrapped_type_v<T> >
> {
    using WrappedT = std::remove_pointer_t<std::remove_reference_t<T>>;

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling pointers to unwrapped types: {}", demangle<T>());
        if (i >= info.Length()) {
            set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple);

        } else {
            stuff.emplace_back(std::make_unique<Stuff<T>>(CastToNative<T>()(info.GetIsolate(), info[i++])));
        }
        return static_cast<Stuff<WrappedT> &>(*stuff.back()).get();
    }
};



/**
* Pointers and references to wrapped types.
* Wrapped types already have a pointer to memory for an existing C++ object in side the javascript object, so no need
* to populate `stuff`
*/
template <class T>
struct ParameterBuilder<T*, std::enable_if_t<is_wrapped_type_v<T> >
> {
    using WrappedT = std::remove_pointer_t<std::remove_reference_t<T>>;

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling pointer to wrapped type: {}", demangle<T>());

        //std::cerr << fmt::format("ParameterBuilder type: pointer-to {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
        if (i >= info.Length()) {
            //set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple);
            throw CastException("Default parameters for wrapped types not currently supported");
        } else {
            return CastToNative<T *>()(info.GetIsolate(), info[i++]);

        }
    }
};



template<class T>
struct ParameterBuilder<T, std::enable_if_t<!std::is_pointer_v<T> && !std::is_reference_v<T> && !is_wrapped_type_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
               std::vector<std::unique_ptr<StuffBase>> & stuff,
               DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling type: {}", demangle<T>());
        if (i < info.Length()) {
            return CastToNative<T>()(info.GetIsolate(), info[i++]);

        } else if constexpr(default_arg_position >= 0 && default_arg_position < std::tuple_size_v<DefaultArgsTuple>) {

            std::cerr << fmt::format("getting default value from default args tuple: {} => {}",
                                     demangle<T>(),
                                     std::get<(std::size_t) default_arg_position>(default_args_tuple)) << std::endl;

            return std::get<(std::size_t) default_arg_position>(default_args_tuple);
        } else {
            throw CastException("Not enough parameters and no default value for {}", demangle<T>());
        }
    }
};




template<>
struct ParameterBuilder<char *> {
    using T = char *;

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    char * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                      std::vector<std::unique_ptr<StuffBase>> & stuff,
                      DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling char *");

        //std::cerr << fmt::format("ParameterBuilder type: char* default_arg_position = {}", default_arg_position) << std::endl;

        // if there is a value, use it, otherwise just use empty string
        if (i >= info.Length()) {
            return set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple).get();

        } else {
            auto string = CastToNative<T>()(info.GetIsolate(), info[i++]);
            stuff.emplace_back(std::make_unique<Stuff<decltype(string)>>(std::move(string)));
            return static_cast<Stuff<decltype(string)> &>(*stuff.back()).get()->get();
        }
    }

};


template<template<class, class...> class Container, class... Rest>
struct ParameterBuilder<Container<char *, Rest...>, std::enable_if_t<!std::is_reference<std::result_of_t<
    CastToNative<std::remove_const_t<std::remove_reference_t<Container<char *, Rest...>>>>(v8::Isolate *,
                                                                                           v8::Local<v8::Value>)
> // end result_of
>::value
>> {
    using ResultType = Container<char *>;
    using IntermediaryType = std::result_of_t<CastToNative<char *>(v8::Isolate *, v8::Local<v8::Value>)>;
    using DataHolderType = Container<IntermediaryType>;


    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    ResultType operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                          std::vector<std::unique_ptr<StuffBase>> & stuffs,
                          DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling container of char *");

        //std::cerr << fmt::format("ParameterBuilder type: Container<char*,...> deafult_arg_position = {}", default_arg_position) << std::endl;

        if (i >= info.Length()) {
            //         static_assert(false, "implement me");
            throw InvalidCallException(fmt::format(
                "Not enough javascript parameters for function call - requires {} but only {} were specified",
                i + 1 + sizeof(Rest)..., info.Length()));
        }
        Stuff<DataHolderType> stuff(CastToNative<ResultType>()(info.GetIsolate(), info[i++]));
        auto data_holder = stuff.get();

        stuffs.emplace_back(std::move(stuff));


        ResultType result;
        for (auto & str : data_holder) {
            result.push_back(str->c_str());
        }
        return result;
    }
};

template<template<class, class...> class Container, class... Rest>
struct ParameterBuilder<Container<char const *, Rest...>,
    std::enable_if_t<!std::is_reference<std::result_of_t<
        CastToNative<std::remove_reference_t<Container<const char *, Rest...>>>(v8::Isolate *, v8::Local<v8::Value>)
    > // end result_of
    >::value
    >> {
    using ResultType = Container<const char *>;
    using IntermediaryType = std::result_of_t<CastToNative<const char *>(v8::Isolate *, v8::Local<v8::Value>)>;
    using DataHolderType = Container<IntermediaryType>;


    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    ResultType operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                          std::vector<std::unique_ptr<StuffBase>> & stuffs,
                          DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling container of char const *");

        //std::cerr << fmt::format("parameterbuilder type: Container<char const *,...> default_arg_position = {}", default_arg_position) << std::endl;
        if (i >= info.Length()) {
//         static_assert(false, "implement me");

            throw InvalidCallException(fmt::format(
                "Not enough javascript parameters for function call - requires {} but only {} were specified",
                i + 1 + sizeof(Rest)..., info.Length()));
        }
        Stuff<DataHolderType> stuff(CastToNative<ResultType>()(info.GetIsolate(), info[i++]));
        auto data_holder = stuff.get();

        stuffs.emplace_back(std::make_unique<Stuff<DataHolderType>>
                                (std::move(stuff)));


        ResultType result;
        for (auto & str : *data_holder) {
            result.push_back(str.get());
        }
        return result;
    }
};


// const char *
template<>
struct ParameterBuilder<char const *> {
    using T = char const *;

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    char const * operator()(const v8::FunctionCallbackInfo<v8::Value> & info,
                            int & i,
                            std::vector<std::unique_ptr<StuffBase>> & stuff,
                            DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling char const *");

        //std::cerr << fmt::format("ParameterBuilder type: char const *, default_arg_position = {}", default_arg_position) << std::endl;

        // if there is a value, use it, otherwise just use empty string
        if (i >= info.Length()) {
            return set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple).get();

        } else {
            auto string = CastToNative<T>()(info.GetIsolate(), info[i++]);
            stuff.emplace_back(std::make_unique<Stuff<decltype(string)>>(std::move(string)));
            return static_cast<Stuff<decltype(string)> &>(*stuff.back()).get()->get();
        }
    }
};


template<>
struct ParameterBuilder<const v8::FunctionCallbackInfo<v8::Value> &> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    const v8::FunctionCallbackInfo<v8::Value> & operator()(const v8::FunctionCallbackInfo<v8::Value> & info,
                                                           int & i,
                                                           std::vector<std::unique_ptr<StuffBase>> & stuff,
                                                           DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling v8::FunctionCallbackInfo<v8::Value> const &");

        return info;
    }
};


template<>
struct ParameterBuilder<v8::Isolate *> {
    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Isolate * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                             std::vector<std::unique_ptr<StuffBase>> & stuff,
                             DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling v8::Isolate *");

        return info.GetIsolate();
    }
};


template<>
struct ParameterBuilder<v8::Local<v8::Context>> {
    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Local<v8::Context> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                                      std::vector<std::unique_ptr<StuffBase>> & stuff,
                                      DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling v8::Local<v8::Context>");

        return info.GetIsolate()->GetCurrentContext();
    }
};


/**
 * If the type wants a JavaScript object directly, pass it through
 * @tparam T
 */
template <class T>
struct ParameterBuilder<
    v8::Local<T>,
    std::enable_if_t<
        !std::is_pointer_v<v8::Local<T>> &&
            !std::is_reference_v<v8::Local<T>> &&
            !is_wrapped_type_v<v8::Local<T>>
    > // enable_if
> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Local<T> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                                     std::vector<std::unique_ptr<StuffBase>> & stuff,
                                     DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling v8::Local<{}>", demangle<T>());

        return v8toolkit::get_value_as<T>(info.This());
    }
};

} // namespace v8toolkit