
#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <tuple>

#include <string.h>

#include "v8helpers.h"
#include "casts.hpp"
#include "stdfunctionreplacement.h"

#ifndef _MSC_VER
#include <dirent.h>
#endif


#define V8_TOOLKIT_DEBUG false


/** TODO LIST
*
* All includes should be included as #include "v8toolkit/FILENAME.h" instead of just #include "FILENAME.h"
* Rename javascript.h file to something much better
* Including "v8toolkit/v8toolkit.h" should include everything, but specific includes should also work for
*   v8helpers.h, <new name for v8toolkit.h>, <new name for javascript.h>
*/



namespace v8toolkit {



    struct SetWeakCallbackData {
        SetWeakCallbackData(func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback,
                            v8::Isolate * isolate,
                            const v8::Local<v8::Object> & javascript_object,
                            bool destructive);

        func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback;
        v8::Global<v8::Object> global;
        bool destructive;
    };


    /**
     * Holds the c++ object to be embedded inside a javascript object along with additional debugging information
     *   when requested
     */
    template<class T>
    struct WrappedData {
        AnyBase * native_object;
        std::string native_object_type = demangle<T>();
        SetWeakCallbackData * weak_callback_data = nullptr;

        WrappedData(T * native_object, SetWeakCallbackData * weak_callback_data) :
            native_object(new AnyPtr<T>(native_object)),
            weak_callback_data(weak_callback_data)
        {}
        ~WrappedData(){delete native_object;}
    };





/* Use these to try to decrease the amount of template instantiations */
#define CONTEXT_SCOPED_RUN(local_context) \
    v8::Isolate * _v8toolkit_internal_isolate = (local_context)->GetIsolate(); \
    v8::Locker _v8toolkit_internal_locker(_v8toolkit_internal_isolate);                \
    v8::Isolate::Scope _v8toolkit_internal_isolate_scope(_v8toolkit_internal_isolate); \
    v8::HandleScope _v8toolkit_internal_handle_scope(_v8toolkit_internal_isolate);     \
    v8::Context::Scope _v8toolkit_internal_context_scope((local_context));

#define GLOBAL_CONTEXT_SCOPED_RUN(isolate, global_context) \
    v8::Locker _v8toolkit_internal_locker(isolate);                \
    v8::Isolate::Scope _v8toolkit_internal_isolate_scope(isolate); \
    v8::HandleScope _v8toolkit_internal_handle_scope(isolate);     \
    /* creating local context must be after creating handle scope */	\
    v8::Local<v8::Context> _v8toolkit_internal_local_context = global_context.Get(isolate); \
    v8::Context::Scope _v8toolkit_internal_context_scope(_v8toolkit_internal_local_context);

#define ISOLATE_SCOPED_RUN(isolate) \
    v8::Locker _v8toolkit_internal_locker((isolate));                \
    v8::Isolate::Scope _v8toolkit_internal_isolate_scope((isolate)); \
    v8::HandleScope _v8toolkit_internal_handle_scope((isolate));

#define DEBUG_SCOPED_RUN(isolate) \
    v8::Locker _v8toolkit_internal_locker((isolate));                \
    v8::Isolate::Scope _v8toolkit_internal_isolate_scope((isolate)); \
    v8::HandleScope _v8toolkit_internal_handle_scope((isolate));     \
    v8::Context::Scope _v8toolkit_internal_context_scope(v8::Debug::GetDebugContext((isolate)));


/**
 * Helper function to run the callable inside contexts.
 * If the isolate is currently inside a context, it will use that context automatically
 *   otherwise no context::scope will be created
 */
template<class T>
auto scoped_run(v8::Isolate * isolate, T callable) -> typename std::result_of<T()>::type
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    if (isolate->InContext()) {
        auto context = isolate->GetCurrentContext();
        v8::Context::Scope context_scope(context);
        return callable();
    } else {
        return callable();
    }
}


/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
* The isolate will be passed to the callable
*/
template<class T>
auto scoped_run(v8::Isolate * isolate, T callable) -> typename std::result_of<T(v8::Isolate*)>::type
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    return callable(isolate);
}

/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
* This version requires the isolate is currently in a context
* The isolate and context will be passed to the callable
*/
template<class T>
auto scoped_run(v8::Isolate * isolate, T callable) -> typename std::result_of<T(v8::Isolate*, v8::Local<v8::Context>)>::type
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    if (isolate->InContext()) {
        auto context = isolate->GetCurrentContext();
        v8::Context::Scope context_scope(context);
        return callable(isolate, context);
    } else {
        throw InvalidCallException("Isolate not currently in a context, but callable expects a context.");
    }
}



// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
*/
template<class T>
auto scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable) -> typename std::result_of<T()>::type
{

    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable();
}


// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
* The isolate will be passed to the callable
*/
template<class T>
auto scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable) -> typename std::result_of<T(v8::Isolate*)>::type
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable(isolate);
}

// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
* The isolate and context will be passed to the callable
*/
template<class T>
auto scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable) ->
	typename std::result_of<T(v8::Isolate*, v8::Local<v8::Context>)>::type
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable(isolate, context);
}

// Same as the ones above, but this one takes a global context for convenience
// Isolate is required since a Local<Context> cannot be created without creating a locker
//   and handlescope which require an isolate to create
template<class T>
auto scoped_run(v8::Isolate * isolate, const v8::Global<v8::Context> & context, T callable)
{
    v8::Locker l(isolate);
    v8::HandleScope hs(isolate);
    auto local_context = context.Get(isolate);
    return scoped_run(isolate, local_context, callable);
}




/**
* When the V8 engine itself generates an error (or a user calls isolate->ThrowException manually with a v8::Value for some reason)
* That exception is re-thrown as a standard C++ exception of this type.   The V8 Value thrown is available.
* get_local_value must be called within a HandleScope
* get_value returns a new Global handle to the value.
*/
class V8Exception : public std::exception {
private:
    v8::Isolate * isolate;
    v8::Global<v8::Value> value;
    std::string value_for_what;

public:
    V8Exception(v8::Isolate * isolate, v8::Global<v8::Value>&& value) : isolate(isolate), value(std::move(value)) {
    std::string str(*v8::String::Utf8Value(this->value.Get(isolate)));
        value_for_what = str == "" ? "unknown error" : str;
    }
    V8Exception(v8::Isolate * isolate, v8::Local<v8::Value> value) : V8Exception(isolate, v8::Global<v8::Value>(isolate, value)) {}
    V8Exception(v8::Isolate * isolate, std::string reason) : V8Exception(isolate, v8::String::NewFromUtf8(isolate, reason.c_str())) {}
    virtual const char * what() const noexcept override {
        return value_for_what.c_str();
    }
    v8::Local<v8::Value> get_local_value(){return value.Get(isolate);}
    v8::Isolate * get_isolate(){return isolate;}
    v8::Global<v8::Value> get_value(){return v8::Global<v8::Value>(isolate, value);}
};


class V8AssertionException : public V8Exception {
public:
    V8AssertionException(v8::Isolate * isolate, v8::Local<v8::Value> value) :
            V8Exception(isolate, value) {}
    V8AssertionException(v8::Isolate * isolate, v8::Global<v8::Value>&& value) :
            V8Exception(isolate, std::forward<v8::Global<v8::Value>>(value)) {}
    V8AssertionException(v8::Isolate * isolate, std::string reason) : V8Exception(isolate, reason) {}
};

class V8ExecutionException : public V8Exception {
    std::string stacktrace;
public:

    V8ExecutionException(v8::Isolate * isolate, v8::TryCatch & tc) :
        V8Exception(isolate, tc.Exception())
    {

        auto stacktrace_maybe = tc.StackTrace(isolate->GetCurrentContext());
        if (!stacktrace_maybe.IsEmpty()) {
            stacktrace = *v8::String::Utf8Value(stacktrace_maybe.ToLocalChecked());
        }
    }
    const std::string & get_stacktrace(){return stacktrace;}
};





    /**
* Same as a V8 exception, except if this type is thrown it indicates the exception was generated
*   during compilation, not at runtime.
*/
class V8CompilationException : public V8Exception {
public:
    V8CompilationException(v8::Isolate * isolate, v8::Global<v8::Value>&& value) :
            V8Exception(isolate, std::forward<v8::Global<v8::Value>>(value)) {}
    V8CompilationException(v8::Isolate * isolate, v8::Local<v8::Value> value) :
            V8Exception(isolate, value) {}
    V8CompilationException(v8::Isolate * isolate, std::string reason) : V8Exception(isolate, reason) {}

};




struct StuffBase{
    // virtual destructor makes sure derived class destructor is called to actually
    //   delete the data
    virtual ~StuffBase(){}
};

template<class T>
struct Stuff : public StuffBase {
    Stuff(T && t) : stuffed(std::make_unique<T>(std::move(t))) {}
    Stuff(std::unique_ptr<T> t) : stuffed(std::move(t)) {}


    static std::unique_ptr<T> stuffer(T && t) {return std::make_unique<T>(std::move(t));}
    static std::unique_ptr<T> stuffer(T const & t) {return std::make_unique<T>(std::move(const_cast<T &>(t)));}

    T * get(){return stuffed.get();}
    std::unique_ptr<T> stuffed;
};



template <class T, class = void>
struct cast_to_native_no_value {
    std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int i) const {
        throw InvalidCallException(fmt::format("Not enough javascript parameters for function call - requires {} but only {} were specified, missing {}", i+1, info.Length(), demangle<T>()));
    }
};


// if the CastToNative is callable with only an isolate, no v8::Value
template <class T>
struct cast_to_native_no_value<T, std::enable_if_t<std::result_of_t<CastToNative<T>()>::value>> {

    T && operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int i) const {
        return CastToNative<T>()();
    }
};




// Helper function for when a required parameter isn't specified in javascript but may have a "global" default value for the type
template <int default_arg_position = -1, class NoRefT, class DefaultArgsTuple = std::tuple<>>
std::enable_if_t<default_arg_position < 0,
        std::result_of_t<cast_to_native_no_value<NoRefT>(const v8::FunctionCallbackInfo<v8::Value> &, int)> &>

set_unspecified_parameter_value(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
                                                                           DefaultArgsTuple && default_args_tuple) {

        // for string types, the result returned may be a unique_ptr in order to save memory for it, this is that type
        using ResultT = std::remove_reference_t<decltype(cast_to_native_no_value<NoRefT>()(info, i++))>;
        stuff.emplace_back(std::make_unique<Stuff<ResultT>>(std::move(cast_to_native_no_value<NoRefT>()(info, i++))));

        // stuff.back() returns a unique_ptr<StuffBase>
        return *((static_cast<Stuff<ResultT> *>(stuff.back().get()))->get());

}


// Helper function for when a required parameter isn't specified in ajvascript but has a function-specific default value specified for it
template <int default_arg_position = -1, class NoRefT, class DefaultArgsTuple = std::tuple<>>
std::enable_if_t<(default_arg_position >= 0),
        std::result_of_t<cast_to_native_no_value<NoRefT>(const v8::FunctionCallbackInfo<v8::Value> &, int)> &>

set_unspecified_parameter_value(const v8::FunctionCallbackInfo<v8::Value> & info,
                                                                              int & i,
                                                                              std::vector<std::unique_ptr<StuffBase>> & stuff,
                                                                              DefaultArgsTuple && default_args_tuple) {

    using ResultT = std::remove_reference_t<decltype(cast_to_native_no_value<NoRefT>()(info, i++))>;
//
//    stuff.emplace_back(
//            std::make_unique<Stuff<ResultT>>(std::get<(std::size_t)default_arg_position>(std::move(default_args_tuple)))
//    );
//
//    return *static_cast<Stuff<ResultT> *>(stuff.back().get())->get();

    // converting the value to a javascript value and then back is an ugly hack to deal with some parameter types changing
    // types when being returned in order to store the memory somewhere it will be cleaned up (like char*)
    auto temporary_js_value = v8toolkit::CastToJS<NoRefT>()(info.GetIsolate(), std::get<(std::size_t)default_arg_position>(std::move(default_args_tuple)));
    stuff.emplace_back(std::make_unique<Stuff<ResultT>>(CastToNative<NoRefT>()(info.GetIsolate(), temporary_js_value)));

    return *((static_cast<Stuff<ResultT> *>(stuff.back().get()))->get());
}


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
struct ParameterBuilder<T*, std::enable_if_t< std::is_fundamental<T>::value >> {


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

      //std::cerr << fmt::format("ParameterBuilder type: pointer-to {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
        if (i >= info.Length()) {
            set_unspecified_parameter_value<default_arg_position, T>(info, i, stuff, default_args_tuple);

        } else {
            stuff.emplace_back(std::make_unique<Stuff < T>>(CastToNative<T>()(info.GetIsolate(), info[i++])));
        }
        return static_cast<Stuff<T> &>(*stuff.back()).get();
    }
};



/**
 * If CastToNative returns a reference
 */
template<class T>
struct ParameterBuilder<T,
    std::enable_if_t<is_wrapped_type_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
                   DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

//        std::cerr << fmt::format("ParameterBuilder type that returns a reference: {} default_arg_position = {}", v8toolkit::demangle<T>(), default_arg_position) << std::endl;
	    
        if (i >= info.Length()) {
            return cast_to_native_no_value<NoRefT>()(info, i++);
        } else {
            return CastToNative<NoRefT>()(info.GetIsolate(), info[i++]);
        }
    }
};


template<class T>
struct remove_const_from_reference {
    using type = T;
};


template<class T>
struct remove_const_from_reference<T const &>{
    using type = T &&;
};

template<class T>
struct remove_const_from_reference<T const>{
    using type = T;
};


template<class T>
using remove_const_from_reference_t = typename remove_const_from_reference<T>::type;



template<class T>
struct ParameterBuilder<T,
    std::enable_if_t<!is_wrapped_type_v<T>>> {
    using NoRefT = std::remove_reference_t<T>;
    using NoConstRefT = std::remove_const_t<NoRefT>;


    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
    T & operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
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
    char * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
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
     CastToNative<std::remove_const_t<std::remove_reference_t<Container<char *, Rest...>>>>(v8::Isolate*, v8::Local<v8::Value>)
        > // end result_of
        >::value
        >> {
     using ResultType = Container<char *>;
     using IntermediaryType = std::result_of_t<CastToNative<char *>(v8::Isolate *, v8::Local<v8::Value>)>;
     using DataHolderType = Container<IntermediaryType>;


     
         template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
     ResultType operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuffs,
                           DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

	   //std::cerr << fmt::format("ParameterBuilder type: Container<char*,...> deafult_arg_position = {}", default_arg_position) << std::endl;
	   
         if (i >= info.Length()) {
    //         static_assert(false, "implement me");
             throw InvalidCallException(fmt::format("Not enough javascript parameters for function call - requires {} but only {} were specified", i+1 + sizeof(Rest)..., info.Length()));
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
             CastToNative<std::remove_reference_t<Container<const char *, Rest...>>>(v8::Isolate*, v8::Local<v8::Value>)
        > // end result_of
        >::value
        >> {
     using ResultType = Container<const char *>;
     using IntermediaryType = std::result_of_t<CastToNative<const char *>(v8::Isolate *, v8::Local<v8::Value>)>;
     using DataHolderType = Container<IntermediaryType>;


     

         template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
         ResultType operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuffs,
                           DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
	   //std::cerr << fmt::format("parameterbuilder type: Container<char const *,...> default_arg_position = {}", default_arg_position) << std::endl;
             if (i >= info.Length()) {
//         static_assert(false, "implement me");

                 throw InvalidCallException(fmt::format(
                     "Not enough javascript parameters for function call - requires {} but only {} were specified",
                     i + 1 + sizeof(Rest)..., info.Length()));
             }
             Stuff < DataHolderType > stuff(CastToNative<ResultType>()(info.GetIsolate(), info[i++]));
             auto data_holder = stuff.get();

             stuffs.emplace_back(std::make_unique<Stuff < DataHolderType>>
             (std::move(stuff)));


             ResultType result;
             for (auto &str : *data_holder) {
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
    v8::Isolate * operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
                             DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
        return info.GetIsolate();
    }
};


template<>
struct ParameterBuilder<v8::Local<v8::Context>> {
    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>>
    v8::Local<v8::Context> operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i, std::vector<std::unique_ptr<StuffBase>> & stuff,
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


template<class ReturnType, class... Args, class... Ts>
auto run_function(func::function<ReturnType(Args...)> & function,
              const v8::FunctionCallbackInfo<v8::Value> & info,
              Ts&&... ts) -> ReturnType {

    return function(std::forward<Args>(ts)...);
}


template<class ReturnType, class... Args, class Callable, class... Ts>
auto run_function(Callable callable,
                  const v8::FunctionCallbackInfo<v8::Value> & info,
                  Ts&&... ts) -> ReturnType {
    return callable(std::forward<Args>(ts)...);
};


template<class Function, class... T>
struct CallCallable;


/**
 * Call a function where the first argument is specified differently for "fake methods" - functions
 * that aren't proper member instance functions but act like it by taking a pointer to "this" as their first
 * explicit parameter
 */
template<class ReturnType, class InitialArg, class... Args>
struct CallCallable<func::function<ReturnType(InitialArg, Args...)>, InitialArg> {
    using NonConstReturnType = std::remove_const_t<ReturnType>;

    template<class DefaultArgsTuple = std::tuple<>, std::size_t... ArgIndexes>
    void operator()(func::function<ReturnType(InitialArg, Args...)> & function,
                    const v8::FunctionCallbackInfo<v8::Value> & info,
                    InitialArg initial_arg,
                    std::index_sequence<ArgIndexes...>,
                    DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {

        int i = 0;

        constexpr int user_parameter_count = sizeof...(Args);
        constexpr int default_arg_count = std::tuple_size<DefaultArgsTuple>::value;

        // while this is negative, no default argument is available
        constexpr int default_parameter_position = default_arg_count - user_parameter_count - 1;

        std::vector<std::unique_ptr<StuffBase>> stuff;
        info.GetReturnValue().
	    Set(v8toolkit::CastToJS<ReturnType>()(info.GetIsolate(),
						  run_function(function, info, std::forward<InitialArg>(initial_arg),
							       std::forward<Args>(
                                       ParameterBuilder<
                                           Args>().template operator()<ArgIndexes - default_arg_count>(info, i, stuff, default_args_tuple))...)));
    }
};


template<class InitialArg, class... Args>
struct CallCallable<func::function<void(InitialArg, Args...)>, InitialArg> {

    template<int default_arg_position, class DefaultArgsTuple = std::tuple<>, std::size_t... ArgIndexes>
    void operator()(func::function<void(InitialArg, Args...)> & function,
                    const v8::FunctionCallbackInfo<v8::Value> & info,
                    InitialArg initial_arg,
                    std::index_sequence<ArgIndexes...>,
                    DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {

        int i = 0;
        constexpr auto default_arg_count = std::tuple_size<DefaultArgsTuple>::value;

        std::vector<std::unique_ptr<StuffBase>> stuff;
        run_function(function, info, std::forward<InitialArg>(initial_arg),
                 std::forward<Args>(ParameterBuilder<Args>().template operator()<ArgIndexes - default_arg_count - 1>(info, i, stuff,
                                                             default_args_tuple))...);

    }
};


/**
 * CallCallable for a normal function which has a non-void return type
 */
template<class ReturnType, class... Args>
struct CallCallable<func::function<ReturnType(Args...)>> {
    using NonConstReturnType = std::remove_const_t<ReturnType>;

    template<class... Ts, class DefaultArgsTuple = std::tuple<>, std::size_t... ArgIndexes>
    void operator()(func::function<ReturnType(Args...)> & function,
                    const v8::FunctionCallbackInfo<v8::Value> & info,
                    std::index_sequence<ArgIndexes...>,
                    DefaultArgsTuple && default_args_tuple = DefaultArgsTuple(),
                    bool return_most_derived = false) {

        int i = 0;

        constexpr int user_parameter_count = sizeof...(Args);
        constexpr int default_arg_count = std::tuple_size<std::remove_reference_t<DefaultArgsTuple>>::value;

        // while this is negative, no default argument is available

	// if there are 3 parameters, 1 default parameter (3-1=2), calls to ParameterBuilder will have positions:
	// 0 - 2 = -2 (no default available)
	// 1 - 2 = -1 (no default available)
	// 2 - 2 = 0 (lookup at std::get<0>(default_args_tuple)
        constexpr int minimum_user_parameters_required = user_parameter_count - default_arg_count;



        std::vector<std::unique_ptr<StuffBase>> stuff;

        info.GetReturnValue().Set(v8toolkit::CastToJS<std::remove_reference_t<ReturnType>>()(info.GetIsolate(),
                                                                    run_function(function, info, std::forward<Args>(
                                                                        ParameterBuilder<Args>().template operator()
                                                                        <(((int)ArgIndexes) - minimum_user_parameters_required), DefaultArgsTuple> (info, i,
                                                                                                 stuff, std::move(default_args_tuple)
                                                                        ))...)));
    }
};


/**
 * call callable for a normal function with a void return
 */
template<class... Args>
struct CallCallable<func::function<void(Args...)>> {

    template<class DefaultArgsTuple = std::tuple<>, std::size_t... ArgIndexes>
    void operator()(func::function<void(Args...)> & function,
                    const v8::FunctionCallbackInfo<v8::Value> & info,
                    std::index_sequence<ArgIndexes...>,
                    DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {

        int i = 0;

        constexpr int user_parameter_count = sizeof...(Args);
        constexpr int default_arg_count = std::tuple_size<DefaultArgsTuple>::value;

        // while this is negative, no default argument is available

	// if there are 3 parameters, 1 default parameter (3-1=2), calls to ParameterBuilder will have positions:
	// 0 - 2 = -2 (no default available)
	// 1 - 2 = -1 (no default available)
	// 2 - 2 = 0 (lookup at std::get<0>(default_args_tuple)
        constexpr int minimum_user_parameters_required = user_parameter_count - default_arg_count;

        std::vector<std::unique_ptr<StuffBase>> stuff;
        run_function(function, info,
                     std::forward<Args>(ParameterBuilder<Args>().
					template operator()<(((int)ArgIndexes) - minimum_user_parameters_required), DefaultArgsTuple>(info, i, stuff, std::move(default_args_tuple)))...);
    }
};


/**
 * CallCallable for a function directly taking a v8::FunctionCallbackInfo
 * This requires the function to do everything itself in terms of parsing parameters
 */
template<>
struct CallCallable<func::function<void(const v8::FunctionCallbackInfo<v8::Value>&)>> {

    template<class DefaultArgsTuple = std::tuple<>, std::size_t... ArgIndexes>
    void operator()(func::function<void(const v8::FunctionCallbackInfo<v8::Value>&)> & function,
                    const v8::FunctionCallbackInfo<v8::Value> & info,
                    std::index_sequence<ArgIndexes...>,
                    DefaultArgsTuple const & default_args_tuple = DefaultArgsTuple()) {
        static_assert(std::is_same<DefaultArgsTuple, std::tuple<>>::value,
                      "function taking a v8::FunctionCallbackInfo object cannot have default parameters");
        function(info);
    }
};


template<class R, class... Args>
struct FunctionTemplateData {
    func::function<R(Args...)> callable;
    std::string name;
};




/**
* Creates a function template from a func::function
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate,
                                                       func::function<R(Args...)> f,
                                                       std::string const & name)
{
    auto data = new FunctionTemplateData<R, Args...>();
    data->callable = f;
    data->name = name;

    // wrap the actual call in this lambda
    return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();

        FunctionTemplateData<R, Args...> & data = *(FunctionTemplateData<R, Args...> *)v8::External::Cast(*(info.Data()))->Value();

        try {
            CallCallable<decltype(data.callable)>()(data.callable, info, std::index_sequence_for<Args...>{});

        } catch (std::exception & e) {

            isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));

            // OLD CODE PUSHED EXCEPTION BACK THROUGH JAVASCRIPT TO C++ BUT EXCEPTION NOT AVAILABLE IN JAVASCRIPT
            //auto anyptr_t = new Any<std::exception_ptr>( std::current_exception());
            // always put in the base ptr so you can cast to it safely and then use dynamic_cast to try to figure
            //   out what it really is
            //isolate->ThrowException(v8::External::New(isolate, static_cast<AnyBase*>(anyptr_t)));
        }
        return; // no return value, PB sets it in the 'info' object

    }, v8::External::New(isolate, (void*)data));
}


/**
* Takes an arbitrary class method and returns a func::function wrapping it
*/
template<class R, class CLASS, class... Args>
func::function<R(Args...)> make_std_function_from_callable(R(CLASS::*f)(Args...) const, CLASS callable)
{
    return func::function<R(Args...)>(callable);
}



template<class R, class... Args>
func::function<R(Args...)> make_std_function_from_callable(R(*callable)(Args...), std::string name) {
    return func::function<R(Args...)>(callable);
};


/**
* Creates a v8::FunctionTemplate for an arbitrary callable
*/
template<class T>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, T callable, std::string name)
{
    return make_function_template(isolate, make_std_function_from_callable(&T::operator(), callable), name);
}


/**
* Creates a function template from a c-style function pointer
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate,  R(*f)(Args...), std::string const & name)
{
    return make_function_template(isolate, func::function<R(Args...)>(f), name);
}


/**
* Helper to both create a function template from a func::function and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, func::function<R(Args...)> function) {
    object_template->Set(isolate, name, make_function_template(isolate, function, name));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class T>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T callable) {
        decltype(LTG<T>::go(&T::operator())) f(callable);
    object_template->Set(isolate, name, make_function_template(isolate, f, name));
}

/**
* Helper to both create a function template from an arbitrary function pointer and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, R(*function)(Args...)) {
    object_template->Set(isolate, name, make_function_template(isolate, function, name));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object allows adding a function to any object, including a context's global object.
*/

template<class T>
void add_function(const v8::Local<v8::Context> & context, const v8::Local<v8::Object> & object, const char * name, T callable)
{
    CONTEXT_SCOPED_RUN(context);

    auto isolate = context->GetIsolate();
    auto function_template = make_function_template(isolate, callable, name);
    auto function = function_template->GetFunction();
    (void)object->Set(context, v8::String::NewFromUtf8(isolate, name), function);
}

/**
* Makes the given javascript value available to all objects created with the object_template as name.
* Often used to populate the object_template used to create v8::Context objects so the variable is available from
*   all contexts created from that object_template
*/
void add_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, const v8::Local<v8::Data> value);


/**
* Makes the given javascript value available in the given object as name.
* Often used to add a variable to a specific context's global object
*/
void add_variable(const v8::Local<v8::Context> context,
                  const v8::Local<v8::Object> & object,
                  const char * name,
                  const v8::Local<v8::Value> value);



/**
* add a function that directly handles the v8 callback data
* explicit function typing needed to coerce non-capturing lambdas into c-style function pointers
*/
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&));

template<int position, class Tuple>
struct TupleForEach;

/**
* Populates an array of v8::Values with the values of the tuple, casted by the tuple element's type
*/
template<int position, class Tuple>
struct TupleForEach : public TupleForEach<position - 1, Tuple> {
    using super = TupleForEach<position - 1, Tuple>;
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> * params, const Tuple & tuple){
        constexpr int array_position = position - 1;
        params[array_position] = CastToJS<typename std::tuple_element<array_position, Tuple>::type>()(isolate, std::get<array_position>(tuple));
        super::operator()(isolate, params, tuple);
    }
};

/**
* Base case for no remaining elements to parse (as determined by the position being 0)
*/
template<class Tuple>
struct TupleForEach<0, Tuple> {
  void operator()(v8::Isolate *, v8::Local<v8::Value> *, const Tuple &){}
};


/**
 *
 */
template<class... OriginalTypes, class... Ts>
v8::Local<v8::Value> call_javascript_function_with_vars(const v8::Local<v8::Context> context,
                                                        const v8::Local<v8::Function> function,
                                                        const v8::Local<v8::Object> receiver,
                                                        const TypeList<OriginalTypes...> & type_list,
                                                        Ts&&... ts) {
    auto isolate = context->GetIsolate();
    std::vector<v8::Local<v8::Value>> parameters {CastToJS<OriginalTypes>()(isolate, std::forward<Ts>(ts))...};
    auto parameter_count = sizeof...(ts);
    v8::TryCatch tc(isolate);
    auto maybe_result = function->Call(context, receiver, parameter_count, &parameters[0]);
    if(tc.HasCaught() || maybe_result.IsEmpty()) {
        printf("Error running javascript function: '%s'\n", *v8::String::Utf8Value(tc.Exception()));
        if (v8toolkit::static_any<std::is_const<std::remove_reference_t<OriginalTypes>>::value...>::value) {
            printf("Some of the types are const, make sure what you are using them for is available on the const type\n");
        }
	    ReportException(isolate, &tc);
        throw V8ExecutionException(isolate, tc);
    }
    return maybe_result.ToLocalChecked();

}

/**
* Returns true on success with the result in the "result" parameter
*/
template<class TupleType = std::tuple<>>
v8::Local<v8::Value> call_javascript_function(const v8::Local<v8::Context> context,
                              const v8::Local<v8::Function> function,
                              const v8::Local<v8::Object> receiver,
                              const TupleType & tuple = {})
{
    constexpr int tuple_size = std::tuple_size<TupleType>::value;
    std::array<v8::Local<v8::Value>, tuple_size> parameters;
    auto isolate = context->GetIsolate();
    TupleForEach<tuple_size, TupleType>()(isolate, parameters.data(), tuple);

    v8::TryCatch tc(isolate);

    // printf("\n\n**** Call_javascript_function with receiver: %s\n", stringify_value(isolate, v8::Local<v8::Value>::Cast(receiver)).c_str());
    auto maybe_result = function->Call(context, receiver, tuple_size, parameters.data());
    if(tc.HasCaught() || maybe_result.IsEmpty()) {
	ReportException(isolate, &tc);
        printf("Error running javascript function: '%s'\n", *v8::String::Utf8Value(tc.Exception()));
        throw V8ExecutionException(isolate, tc);
    }
    return maybe_result.ToLocalChecked();
}

/**
* Returns true on success with the result in the "result" parameter
*/
template<class TupleType = std::tuple<>>
v8::Local<v8::Value> call_javascript_function(const v8::Local<v8::Context> context,
                              const std::string & function_name,
                              const v8::Local<v8::Object> receiver,
                              const TupleType & tuple = {})
{
    auto maybe_value = receiver->Get(context, v8::String::NewFromUtf8(context->GetIsolate(),function_name.c_str()));
    if(maybe_value.IsEmpty()) {
        throw InvalidCallException(fmt::format("Function name {} could not be found", function_name));
    }

    auto value = maybe_value.ToLocalChecked();
    if(!value->IsFunction()) {
        throw InvalidCallException(fmt::format("{} was found but is not a function", function_name));;
    }

    return call_javascript_function(context, v8::Local<v8::Function>::Cast(value), receiver, tuple);
}


// helper for getting exposed variables
template<class T>
void _variable_getter(v8::Local<v8::String> property,
                      const v8::PropertyCallbackInfo<v8::Value>& info)
{
    auto isolate = info.GetIsolate();
    T * variable = (T*)v8::External::Cast(*(info.Data()))->Value();
//    if (return_most_derived) {
//        //TODO: ME
//        assert(false);
//    } else {
        info.GetReturnValue().Set(CastToJS<T>()(isolate, *variable));
//    }
}


// setter is a no-op if the type is const
template<class T, std::enable_if_t<std::is_const<T>::value, int> = 0>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    // should this throw a V8 exception?
}


// if the type is not const, then set the value as requested
template<class T, std::enable_if_t<!std::is_const<T>::value, int> = 0>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    // TODO: This doesnt work well with pointer types - we want to assign to the dereferenced version, most likely.
    *(T*)v8::External::Cast(*(info.Data()))->Value() = CastToNative<T>()(info.GetIsolate(), value);
}
//
//template <class T>
//struct AccessorData {
//    T * variable;
//    bool return_most_derived = false;
//    AccessorData(T * variable, bool return_most_derived = false) :
//        variable(variable),
//        return_most_derived(return_most_derived)
//    {}
//};

/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads and writes to the variable
*/
template<class T>
void expose_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                                 _variable_getter<T>,
                                 _variable_setter<T>,
                                 v8::External::New(isolate, &variable));
}


template<class T, class... Rest>
 void expose_variable(v8::Isolate * isolate,
                      const v8::Local<v8::ObjectTemplate> & object_template,
                      const char * name,
                      std::unique_ptr<T, Rest...> & variable) {
object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                             _variable_getter<std::unique_ptr<T, Rest...>&>,
                             _variable_setter<std::unique_ptr<T, Rest...>&>,
                             v8::External::New(isolate, variable.get()));
}



/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads to the variable.  Writes are ignored.
* TODO: consider making writes errors (throw?)
*/
template<class T>
void expose_variable_readonly(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                                 _variable_getter<T>,
                                 0,
                                 v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read/write variable
* Often used to add the variable to a context's global object
*/
template<class T>
void expose_variable(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, T & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                        _variable_getter<T>,
                        _variable_setter<T>,
                        v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read-only variable (writes are ignored but are not errors)
* TODO: consider making them errors (throw?)
* Often used to add the variable to a context's global object
*/
template<class T>
void expose_variable_readonly(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, T & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name), _variable_getter<T>, 0, v8::External::New(isolate, &variable));
}




/**
* Takes a local and creates a weak global reference callback for it
* Useful for clearing out C++-allocated memory on javascript garbage collection of an associated javascript object
 * Remember, this is not guaranteed to ever be called
*/
SetWeakCallbackData * global_set_weak(v8::Isolate * isolate,
                     const v8::Local<v8::Object> & javascript_object,
                     func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback,
                      bool destructive);



// takes a format string and some javascript objects and does a printf-style print using boost::format
// fills missing parameters with empty strings and prints any extra parameters with spaces between them
std::string _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline);


/**
* Returns the values in a FunctionCallbackInfo object breaking out first-level arrays into their
*  contained values (but not subsequent arrays for no particular reason)
*/
std::vector<v8::Local<v8::Value>> get_all_values(const v8::FunctionCallbackInfo<v8::Value>& args, int depth = 1);



// prints out arguments with a space between them
std::string _print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline);

/**
* Adds the print functions listed below to the given object_template (usually a v8::Context's global object)
* Optional callback function can be used to send the output to another source (defaults to stdout)
*
* call this to add a set of print* functions to whatever object template you pass in (probably the global one)
* print takes a single variable or an array and prints each value separated by spaces
*
* println same as print but automatically appends a newlines
*
* printf - Treats the first parameter as a format string.
*          any additional values will be used to fill the format string.  If there are insufficient parameters
*          to fill the format, the empty string "" will be used.   Any extra parameters will be printed after
*          the filled format string separated by spaces
*
* printfln - same as printf but automatically appends a newline
*
* printobj - prints a bunch of information about an object - format highly susceptible to frequent change
*/
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> object_template, func::function<void(const std::string &)> = [](const std::string & s){printf("%s", s.c_str());} );
void add_print(const v8::Local<v8::Context> context, func::function<void(const std::string &)> callback = [](const std::string & s){printf("%s", s.c_str());});

/**
* Adds an assert method that calls assert.h assert() on failure.  This is different than the add_assert() in javascript.h that throws an exception on failure
*   because if an exception is not caught before it reaches V8 execution, the program is terminated.  javascript.h *Helper classes automatically catch
*   and re-throw exceptions so it is safe to throw in that version, but not this one.  The error message resulting from throwing an exception
*   reaching code compiled without exception support is not easy to understand which is why a simple assert is preferable.
*/
void add_assert(v8::Isolate * isolate,  v8::Local<v8::ObjectTemplate> object_template);

// returns true if the two values are the same by value, including nested data structures
bool compare_contents(v8::Isolate * isolate, const v8::Local<v8::Value> & left, const v8::Local<v8::Value> & right);


/**
* Accepts an object and a method on that object to be called later via its operator()
* Does not require knowledge of how many parameters the method takes or any placeholder arguments
* Can be wrapped with a func::function
*/
template<class>
struct Bind{};

/**
 * Non-const object to non-const method
 */
template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...)> {

    Bind(T & object, R(T::*method)(Args...) )
        :
      object(object), method(method)
    {}

    ~Bind(){}

    T & object;
    R(T::*method)(Args...);

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
//        return R();
    }
};

template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) &> {

    Bind(T & object, R(T::*method)(Args...) &) :
            object(object), method(method){}

    ~Bind(){}

    T & object;
    R(T::*method)(Args...) &;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};



/**
 * Non-const object to const method
 */
template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) const> {

    Bind(T const & object, R(T::*method)(Args...) const) :
      object(object), method(method){}

    T const & object;
    R(T::*method)(Args...) const;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};


template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) const &> {

    Bind(T const & object, R(T::*method)(Args...) const &) :
            object(object), method(method){}

    T const & object;
    R(T::*method)(Args...) const &;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};





/**
 * unqualified
* Helper function to create a Bind object using type deduction and wrap it in a
* func::function object.
* This specialization is for handling non-const class methods
*/
template <class CLASS, class R, class METHOD_CLASS, class... Args>
    func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))
//    func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))
//    auto bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))

    {
    return Bind<decltype(method)>(object, method);
}


/**
 * l-value qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) &)
{
    return Bind<decltype(method)>(object, method);
}



/**
 * Const qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) const)
{
    return Bind<decltype(method)>(object, method);
}


/**
 * l-value and const qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) const &)
{
    return Bind<decltype(method)>(object, method);
}


/**
* Example allocator code from V8 Docs
*/
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  inline virtual void* Allocate(size_t length) override {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  inline virtual void* AllocateUninitialized(size_t length) override { return malloc(length); }
  inline virtual void Free(void* data, size_t) override { free(data); }
};


/**
* If the filename `filename` exists, reeturns true and sets the last modificaiton time and contents
*   otherwise returns false
*/
bool get_file_contents(std::string filename, std::string & file_contents, time_t & file_modification_time);

/**
* same as longer version, just doesn't return modification time if it's not desired
*/
bool get_file_contents(std::string filename, std::string & file_contents);


/**
 * Before deleting an isolate using require, make sure to clean up
 * its require cache or the program will crash while exiting
 * @param isolate which isolate to delete the cache for
 */
void delete_require_cache_for_isolate(v8::Isolate * isolate);


/**
* adds 'require' method to javascript to emulate node require.
* Adds an self-referential "global" alias to the global object
* Must be run after the context is created so "global" can refer to the global object
*   (if this could be changed, it should be, but I don't know how to do it beforehand)
*/
void add_require(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & context, const std::vector<std::string> & paths);


/**
* adds "module_list()" to javascript to require a dictionary of module path+names to exported objects
*   currently required into the specified isolate
*/
void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template);



struct RequireResult {
    v8::Isolate * isolate;
    v8::Global<v8::Context> context;
    v8::Global<v8::Function> function;
    v8::Global<v8::Value> result;
    time_t time;
    RequireResult(v8::Isolate * isolate,
                  v8::Local<v8::Context> context,
                  v8::Local<v8::Function> function,
                  v8::Local<v8::Value> result,
                  const time_t & time) :
            isolate(isolate),
            context(v8::Global<v8::Context>(isolate, context)),
            function(v8::Global<v8::Function>(isolate, function)),
            result(v8::Global<v8::Value>(isolate, result)),
            time(time)
    {}
    // IF CRASHING IN RequireResult DESTRUCTOR, MAKE SURE TO CALL delete_require_cache_for_isolate BEFORE DESTROYING ISOLATE
};


    /**
* Attempts to load the specified module name from the given paths (in order).
*   Returns the exported object from the module.
* Same as calling require() from javascript - this is the code that is actually run for that
*/
bool require(v8::Local<v8::Context> context,
             std::string filename,
             v8::Local<v8::Value> & result,
             const std::vector<std::string> & paths,
             bool track_modification_times = false,
             bool use_cache = true,
             func::function<void(RequireResult const &)> callback = func::function<void(RequireResult const &)>(),
             func::function<std::string(std::string const &)> resource_name_callback = func::function<std::string(std::string const &)>()
    );


/**
* requires all the files in a directory
*/
void require_directory(v8::Local<v8::Context> context, std::string directory_name);


void dump_prototypes(v8::Isolate * isolate, v8::Local<v8::Object> object);



// void require_directory(std::string directory_name)
// {
//
// // #include <boost/filesystem.hpp>
//     //
//     // boost::filesystem::path p = boost::filesystem::current_path();
//     // boost::filesystem::directory_iterator it{p};
//     // while (it != boost::filesystem::directory_iterator{})
//     //   std::cout << *it++ << '\n';
//     //
//
//     // This probably works on more than just APPLE
// #ifdef __APPLE__
//     DIR * dir = opendir(".");
//     if (dir == NULL)
//             return;
//     struct dirent * dp;
//     while ((dp = readdir(dir)) != NULL) {
//             // if (dp->d_namlen == len && strcmp(dp->d_name, name) == 0) {
//             //         (void)closedir(dir);
//             //         return (FOUND);
//             // }
//     }
//     (void)closedir(dir);
//     return;
//
// #endif // __APPLE__
//
// }

std::vector<std::string> get_interesting_properties(v8::Local<v8::Context> context, v8::Local<v8::Object> object);

v8::Local<v8::Value> run_script(v8::Local<v8::Context> context, v8::Local<v8::Script> script);


void foreach_file(const std::string & directory_name, std::function<void(const std::string &)> const & callback);

void foreach_directory(const std::string & directory_name, std::function<void(const std::string &)> const & callback);

} // end v8toolkit namespace


/** \mainpage v8toolkit API documentation
* v8toolkit is a multi-layer library to simplify working with the V8 Javascript engine.
*
* It contains a set of primitive functions in v8toolkit.h, a library that extends
* the functionality in v8toolkit to user-defined class types in v8_class_wrapper.h,
* and a set of high level classes for integrating V8 with virtually no knowledge of the
* underlying v8 API in javascript.h.
*
* Each of these libraries has internal documentation of its types and functions as well
* as an example usage files (any .cpp file with "sample" in its name).
*/



#include "casts_impl.hpp"
