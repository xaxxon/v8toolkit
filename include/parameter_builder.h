#pragma once

#include <tuple>


#include <v8.h>

#include <xl/demangle.h>

#include "casts_impl.h"
#include "v8helpers.h"
#include "unspecified_parameter_value.h"

namespace v8toolkit {

// define this to log debugging messages while running
//#define V8TOOLKIT_PARAMETER_BUILDER_LOGGING

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
 * If the parameter isn't self contained (pointers, references, and things that behave like a char*, then the memory
 * allocated for it will be stored in `stuff` which is automatically cleaned up when the function returns.
 *
 * If no explicit parameter is provided from JavaScript, a default value may be available in `default_args_tuple`
 */
template<class T, class = void>
struct ParameterBuilder {
    static_assert("No ParameterBuilder for specified type");
};


template<class T>
struct ParameterBuilder<T const> {
    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    const T operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder proxying const {} => {}", xl::demangle<T>(), xl::demangle<T>());
        return ParameterBuilder<T>()(info, i, stuff, std::move(default_args_tuple));
    }
};


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
        PB_PRINT("ParameterBuilder handling lvalue reference: {}", xl::demangle<T>());
        return * ParameterBuilder<T*>()(info, i, stuff, std::move(default_args_tuple));
    }

};


template <class T>
struct ParameterBuilder<T &, std::enable_if_t<std::is_same_v<std::remove_const_t<T>, char> && !is_wrapped_type_v<T>>> {
    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling lvalue reference for char: {}", xl::demangle<T>());
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

        PB_PRINT("ParameterBuilder handling rvalue reference to unwrapped type: {}", xl::demangle<T>());
//        std::cerr << fmt::format("default_arg_position: {}", default_arg_position) << std::endl;
        return std::move(*(ParameterBuilder<T*>().template operator()<default_arg_position>(info, i, stuff, std::move(default_args_tuple))));
    }

};



/**
* Pointers to unwrapped types
*/
template <class T>
struct ParameterBuilder<T*,
    std::enable_if_t<
        !is_wrapped_type_v<T> &&
        !std::is_same_v<std::remove_const_t<T>, char>
    >>
{
    using WrappedT = std::remove_const_t<std::remove_pointer_t<std::remove_reference_t<T>>>;
    static_assert(!std::is_pointer_v<WrappedT>, "multi-pointer types not supported");

    template<int default_arg_position = -1, class DefaultArgsTupleRef = std::tuple<>>
    WrappedT * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTupleRef && default_args_tuple = DefaultArgsTupleRef()) {

        PB_PRINT("ParameterBuilder handling pointers to unwrapped types: {}", xl::demangle<T>());
        if (i >= info.Length()) {
            return get_default_parameter<WrappedT, default_arg_position>(info, i, stuff, default_args_tuple);
        } else {
            stuff.push_back(Stuff<WrappedT>::stuffer(ParameterBuilder<WrappedT>().template operator()<default_arg_position>(info, i, stuff, default_args_tuple)));
            return static_cast<Stuff<WrappedT> &>(*stuff.back()).get();
        }
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
    WrappedT * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                   std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling pointer to wrapped type: {}", xl::demangle<T>());

        //std::cerr << fmt::format("ParameterBuilder type: pointer-to {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
        if (i >= info.Length()) {
            return *get_default_parameter<std::remove_const_t<WrappedT> *, default_arg_position>(info, i, stuff, default_args_tuple);

        } else {
            // try to get the object from inside a javascript object, otherwise, fall back to a CastToNative<T> call
            if (auto wrapped_pointer = CastToNative<WrappedT *>()(info.GetIsolate(), info[i++])) {
                return wrapped_pointer;

            } else if constexpr(CastToNative<WrappedT>::callable()) {
                stuff.push_back(Stuff<WrappedT>::stuffer(CastToNative<WrappedT>()(info.GetIsolate(), info[i++])));
                return static_cast<Stuff<WrappedT> &>(*stuff.back()).get();
            } else {
                throw CastException("Tried to send a pointer to a function but the JavaScript object wasn't a wrapped "
                                        "C++ object and a new object couldn't be created from the JavaScript value provided");
            }
        }
    }

};



template<class T>
struct ParameterBuilder<T, std::enable_if_t<
    !is_v8_type_v<T> && // v8 types passed straight through
    !is_string_not_owning_memory_v<T> && // strings needing memory stored for them are handled differently
        !std::is_pointer_v<T> &&
        !std::is_reference_v<T> &&
        !is_wrapped_type_v<T>>> // wrapped types handled from ParameterBuilder in v8_class_wrapper.h
{
    using NoRefT = std::remove_reference_t<T>;


    template<int default_arg_position = -1, class DefaultArgsTupleRef = std::tuple<>>
    T operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
               std::vector<std::unique_ptr<StuffBase>> & stuff,
               DefaultArgsTupleRef && default_args_tuple = DefaultArgsTupleRef()) {

        using DefaultArgsTuple = std::remove_reference_t<DefaultArgsTupleRef>;

        PB_PRINT("ParameterBuilder handling type: {}", xl::demangle<T>());
        if (i < info.Length()) {
            return CastToNative<T>()(info.GetIsolate(), info[i++]);

        } else if constexpr(default_arg_position >= 0 && default_arg_position < std::tuple_size_v<std::remove_reference_t<DefaultArgsTuple>>) {

//            `                         xl::demangle<std::tuple_element_t<default_arg_position, DefaultArgsTuple>>()) << std::endl;

            return std::get<(std::size_t) default_arg_position>(default_args_tuple);
        } else {
            throw CastException("Not enough parameters and no default value for {}", xl::demangle<T>());
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
            return *get_default_parameter<ResultType, default_arg_position>(info, i, stuffs, default_args_tuple);

        } else {
            stuffs.emplace_back(Stuff<ResultType>::stuffer(CastToNative<ResultType>()(info.GetIsolate(), info[i++])));
            return *static_cast<Stuff<ResultType> &>(*stuffs.back()).get()->get();
        }
    }

};


template<class T>
struct ParameterBuilder<T, std::enable_if_t<is_string_not_owning_memory_v<T>>> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    T operator()(const v8::FunctionCallbackInfo<v8::Value> & info,
                            int & i,
                            std::vector<std::unique_ptr<StuffBase>> & stuff,
                            DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        PB_PRINT("ParameterBuilder handling {}", xl::demangle<T>());

        //std::cerr << fmt::format("ParameterBuilder type: char const *, default_arg_position = {}", default_arg_position) << std::endl;

        // if there is a value, use it, otherwise just use empty string
        if (i >= info.Length()) {
            return T(*get_default_parameter<T, default_arg_position>(info, i, stuff, default_args_tuple));

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


/**
 * If the type wants a raw JavaScript isolate, pass it through
 */
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


/**
 * If the type wants a raw JavaScript context, pass it through.
 */
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
 * If the type wants a raw JavaScript value of any type except v8::object, pass it the current parameter in the list
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
        static_assert(default_arg_position < 0, "Cannot have a default value for a v8::Local<T> parameter");
        PB_PRINT("ParameterBuilder handling v8::Local<{}>", xl::demangle<T>());

        return v8toolkit::get_value_as<T>(info.GetIsolate(), info[i++]);
    }
};
//
///**
// * If the type wants a raw JavaScript object, pass it the receiving objects
// * @tparam T
// */
//template <>
//struct ParameterBuilder<
//    v8toolkit::Holder,
//    std::enable_if_t<
//        !std::is_pointer_v<v8::Local<v8::Object>> &&
//        !std::is_reference_v<v8::Local<v8::Object>> &&
//        !is_wrapped_type_v<v8::Local<v8::Object>>
//    > // enable_if
//> {
//
//    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
//    v8toolkit::Holder operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
//                            std::vector<std::unique_ptr<StuffBase>> & stuff,
//                            DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
//        PB_PRINT("ParameterBuilder handling v8::Local<{}>", xl::demangle<T>());
//
//        // holder is the JavaScript object which is the actual WrappedClass, not something which may have that as a
//        //   prototype
//    }
//};

template <>
struct ParameterBuilder<
    v8toolkit::This,
    std::enable_if_t<
        !std::is_pointer_v<v8::Local<v8::Object>> &&
        !std::is_reference_v<v8::Local<v8::Object>> &&
        !is_wrapped_type_v<v8::Local<v8::Object>>
    > // enable_if
> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8toolkit::This operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
                                     std::vector<std::unique_ptr<StuffBase>> & stuff,
                                     DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        PB_PRINT("ParameterBuilder handling v8::Local<{}>", xl::demangle<T>());

        // holder is the JavaScript object which is the actual WrappedClass, not something which may have that as a
        //   prototype
        return v8toolkit::This(info.This());
    }
};


} // namespace v8toolkit