#pragma once



#include <v8.h>

#include "casts.hpp"
#include "v8helpers.h"
#include "unspecified_parameter_value.h"

namespace v8toolkit {

/**
 * Class for turning a function parameter list into a parameter pack useful for calling the function
 * depth is the current index into the FunctionCallbackInfo object's parameter list
 * Function is the complete type of the function to call
 * TypeList is the types of the remaining parameterse to parse - whe this is empty the function can be called
 */
template<class T, class = void>
struct ParameterBuilder;

/**
* Specialization that deals with pointers to primitive types by creating a holder that the address of can be passed along
*/
template<class T>
struct ParameterBuilder<T *, std::enable_if_t<std::is_fundamental<T>::value >> {


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        //std::cerr << fmt::format("ParameterBuilder type: pointer-to {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
        if (i >= info.Length()) {
            set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple);

        } else {
            stuff.emplace_back(std::make_unique<Stuff<T>>(CastToNative<T>()(info.GetIsolate(), info[i++])));
        }
        return static_cast<Stuff<T> &>(*stuff.back()).get();
    }
};


template<class T>
struct ParameterBuilder<T,
    std::enable_if_t<is_wrapped_type_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T &
    operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
               std::vector<std::unique_ptr<StuffBase>> & stuff,
               DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        //        std::cerr << fmt::format("ParameterBuilder type that returns a reference: {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;

        if (i >= info.Length()) {
            throw CastException("Default values for wrapped types not currently supported");
            //            return cast_to_native_no_value<NoRefT>()(info, i++);
        } else {
            return CastToNative<T&>()(info.GetIsolate(), info[i++]);
        }
    }

};


template<class T>
struct ParameterBuilder<T, std::enable_if_t<is_wrapped_typeish_v<T> && !is_wrapped_type_v<T>>> {

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                 std::vector<std::unique_ptr<StuffBase>> & stuff,
                 DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        if (i >= info.Length()) {
            throw CastException("Default parameters not handled for wrapped types");
        } else {
            return CastToNative<T>()(info.GetIsolate(), info[i++]);
        }
    }
};


template<class T>
struct ParameterBuilder<T,
    std::enable_if_t<!is_wrapped_typeish_v<T> && !std::is_pointer_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;
    using NoConstRefT = std::remove_const_t<NoRefT>;


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        //std::cerr << fmt::format("ParameterBuilder type: {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
        if (i >= info.Length()) {
            set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple);
        } else {


            // since this is an unwrapped class, we know that CastToNative is returning an rvalue
            auto up = Stuff<NoConstRefT>::stuffer(CastToNative<T>()(info.GetIsolate(), info[i++]));
            stuff.emplace_back(std::make_unique<Stuff<NoConstRefT>>(std::move(up)));
        }
        return *static_cast<Stuff<NoConstRefT> &>(*stuff.back()).get();
    }
};


template<>
struct ParameterBuilder<char *> {
    using T = char *;

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    char * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                      std::vector<std::unique_ptr<StuffBase>> & stuff,
                      DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

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
        return info;
    }
};


template<>
struct ParameterBuilder<v8::Isolate *> {
    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Isolate * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                             std::vector<std::unique_ptr<StuffBase>> & stuff,
                             DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        return info.GetIsolate();
    }
};


template<>
struct ParameterBuilder<v8::Local<v8::Context>> {
    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Local<v8::Context> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                                      std::vector<std::unique_ptr<StuffBase>> & stuff,
                                      DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        return info.GetIsolate()->GetCurrentContext();
    }
};


template<>
struct ParameterBuilder<v8::Local<v8::Object>> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Local<v8::Object> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                                     std::vector<std::unique_ptr<StuffBase>> & stuff,
                                     DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        return info.This();
    }
};

} // namespace v8toolkit