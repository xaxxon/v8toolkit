#pragma once

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <set>
#include <typeinfo>
#include <type_traits>

#include <fmt/ostream.h>

#include <libplatform/libplatform.h>
#include <v8.h>

#include <xl/zstring_view.h>
#include <xl/log.h>

#include "type_traits.h"
#include "stdfunctionreplacement.h"
#include "cast_to_native.h"

#define constexpr

// if it can be determined safely that cxxabi.h is available, include it for name demangling
#if defined __has_include
#if __has_include(<cxxabi.h>)
#  define V8TOOLKIT_DEMANGLE_NAMES
#  include <cxxabi.h>
#endif
#endif

#include <boost/serialization/strong_typedef.hpp>

namespace v8toolkit {

inline bool operator<(v8::Local<v8::Object> const &, v8::Local<v8::Object> const &) {
    return false;
}



class LoggingSubjects {
private:
    inline static std::string subject_names[] = {"Object Management", "Runtime Exception", "Compilation Exception"};

public:

    enum class Subjects {
        V8_OBJECT_MANAGEMENT, // when core V8 objects are created or town down
        RUNTIME_EXCEPTION,
        COMPILATION_EXCEPTION
    };


    std::string const & get_subject_name(Subjects subject) {
        return subject_names[static_cast<std::underlying_type_t<Subjects>>(subject)];
    }
};



inline xl::Log<xl::log::DefaultLevels, v8toolkit::LoggingSubjects> log;


/**
 * class used to signify the caller wants the this object of a function call.
 * Note: the `this` object may not have a wrapped C++ class containing it, even when calling a C++ function.  If a native
 * JavaScript object has another JavaScript object in its prototype chain, that may be where it's getting the function.
 * If you always want that object, instead, use Holder, not This
 */
BOOST_STRONG_TYPEDEF(v8::Local<v8::Object>, This)




template<class MemberT, class ClassT>
    constexpr bool get_member_is_readonly(MemberT(ClassT::*member)) {
        return std::is_const_v<MemberT>;
    };


    template<auto member>
    constexpr bool is_pointer_to_const_data_member_v = get_member_is_readonly(member);



    using StdFunctionCallbackType = func::function<void(const v8::FunctionCallbackInfo<v8::Value>& info)> ;
    struct MethodAdderData {
        std::string method_name;
        StdFunctionCallbackType callback;

        MethodAdderData();
        MethodAdderData(std::string const &, StdFunctionCallbackType const &);
    };

    /**
    * Returns a string with the given stack trace and a leading and trailing newline
    * @param stack_trace stack trace to return a string representation of
    * @return string representation of the given stack trace
    */
    std::string get_stack_trace_string(v8::Local<v8::StackTrace> stack_trace);


    namespace literals {

        // put the following in your code to use these:
        //     using namespace v8toolkit::literals;
        inline v8::Local<v8::String> operator "" _v8(char const * string, unsigned long) {
            v8::Isolate * isolate = v8::Isolate::GetCurrent();
            return v8::String::NewFromUtf8(isolate, string);
        }
        inline v8::Local<v8::Number> operator"" _v8(long double number) {
            v8::Isolate * isolate = v8::Isolate::GetCurrent();
            return v8::Number::New(isolate, number);
        }
        inline v8::Local<v8::Integer> operator"" _v8(unsigned long long int number) {
            v8::Isolate * isolate = v8::Isolate::GetCurrent();
            return v8::Integer::New(isolate, number);
        }

    }



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




// thrown when data cannot be converted properly
class CastException : public std::exception {
private:
    std::string reason;

public:
    template<class... Ts>
    CastException(const std::string & reason, Ts&&... ts) : reason(fmt::format(reason, std::forward<Ts>(ts)...) + get_stack_trace_string(v8::StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 100))) {}
    virtual const char * what() const noexcept override {return reason.c_str();}
};

template<typename T, typename = void>
struct ProxyType {
    using PROXY_TYPE = T;
};

template<typename T>
struct ProxyType<T, std::void_t<typename T::V8TOOLKIT_PROXY_TYPE>>{
    using PROXY_TYPE = typename T::V8TOOLKIT_PROXY_TYPE;
};

void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch);

// use V8TOOLKIT_MACRO_TYPE instead
#define V8TOOLKIT_COMMA ,

// protects and allows subsequent calls to additional macros for types with commas (templated types)
#define V8TOOLKIT_MACRO_TYPE(...) __VA_ARGS__



/**
 * Returns a demangled version of the typeid(T).name() passed in if it knows how,
 *   otherwise returns the mangled name exactly as passed in
 */
std::string demangle_typeid_name(const std::string & mangled_name);

template<class T>
std::string & demangle(){
    static std::string cached_name;
    std::atomic<bool> cache_set = false;

    if (cache_set) {
        return cached_name;
    } else {
        static std::mutex mutex;

        std::lock_guard<std::mutex> lock_guard(mutex);
        if (!cache_set) {
            auto demangled_name = demangle_typeid_name(typeid(T).name());
            std::string constness = std::is_const<T>::value ? "const " : "";
            std::string volatility = std::is_volatile<T>::value ? "volatile " : "";
            cached_name = constness + volatility + demangled_name;
            cache_set = true;
        }
    }

    return cached_name;
 }

// polymorphic types work just like normal
template<class Destination, class Source, std::enable_if_t<std::is_polymorphic<Source>::value, int> = 0>
Destination safe_dynamic_cast(Source * source) {
    static_assert(std::is_pointer<Destination>::value, "must be a pointer type");
    static_assert(!std::is_pointer<std::remove_pointer_t<Destination>>::value, "must be a single pointer type");
//    fprintf(stderr, "safe dynamic cast doing real cast\n");
    return dynamic_cast<Destination>(source);
};


// trivial casts always succeed even if the type isn't polymoprhic
template<class Destination>
Destination * safe_dynamic_cast(Destination * source) {
    return source;
}


// if casting to a base type, it doesn't matter if the type is polymorphic
template<class Destination, class Source,
    std::enable_if_t<
        !std::is_polymorphic_v<Source> && std::is_base_of_v<Destination, Source>
    > * = nullptr>
Destination safe_dynamic_cast(Source * source) {
    return static_cast<Destination>(source);
}

// casting to a derived type from a non-polymorphic type will always fail
template<class Destination, class Source,
    std::enable_if_t<
        !std::is_polymorphic_v<Source> && !std::is_base_of_v<Destination, Source>
    > * = nullptr>
Destination safe_dynamic_cast(Source * source) {
    static_assert(std::is_pointer<Destination>::value, "must be a pointer type");
    static_assert(!std::is_pointer<std::remove_pointer_t<Destination>>::value, "must be a single pointer type");
//    fprintf(stderr, "safe dynamic cast doing fake/stub cast\n");
    return nullptr;
};



template <class... > struct TypeList {};

// for use inside a decltype only
template <class R, class... Ts>
auto get_typelist_for_function(func::function<R(Ts...)>) ->TypeList<Ts...>;

// for use inside a decltype only
template <class R, class Head, class... Tail>
auto get_typelist_for_function_strip_first(func::function<R(Head, Tail...)>) -> TypeList<Tail...>;

// for use inside a decltype only
template <class... Ts>
auto get_typelist_for_variables(Ts... ts) -> TypeList<Ts...>;

template <class... Ts>
auto make_tuple_for_variables(Ts&&... ts) -> std::tuple<Ts...> {
    return std::tuple<Ts...>(std::forward<Ts>(ts)...);
}


template<bool... b> struct static_any;

template<bool... tail>
struct static_any<true, tail...> : std::true_type {};

template<bool... tail>
struct static_any<false, tail...> : static_any<tail...> {};

template<>
struct static_any<> : std::false_type {};

template<bool... bools>
constexpr bool static_any_v = static_any<bools...>::value;


template <bool... b> struct static_all_of;

// If the first parameter is true, look at the rest of the list
template <bool... tail>
struct static_all_of<true, tail...> : static_all_of<tail...> {};

// if any parameter is false, return false
template <bool... tail>
struct static_all_of<false, tail...> : std::false_type {};

// If there are no parameters left, no false was found so return true
template <> struct static_all_of<> : std::true_type {};

template<bool... bools>
constexpr bool static_all_of_v = static_all_of<bools...>::value;


class Exception : public std::exception {
protected:
    std::string message;

public:
    template<class... Ts>
    Exception(std::string const & format, Ts&&... args) : message(fmt::format(format, args...))
    {}

    Exception(){}

    virtual const char * what() const noexcept override {return message.c_str();}

};

/**
* General purpose exception for invalid uses of the v8toolkit API
*/
class InvalidCallException : public Exception {

public:
    InvalidCallException(const std::string & message);
    virtual const char * what() const noexcept override {return message.c_str();}
};


/**
 * Thrown when trying to register a function/method/member with the same name as
 * something else already registered
 */
class DuplicateNameException : public Exception {

public:
    using Exception::Exception;
    virtual const char * what() const noexcept override {return message.c_str();}
};


class UndefinedPropertyException : public Exception {

public:
    using Exception::Exception;
 virtual const char * what() const noexcept override {return message.c_str();}


};

/**
* prints out a ton of info about a v8::Value
*/
void print_v8_value_details(v8::Local<v8::Value> local_value);


/**
* Returns the length of an array
*/
int get_array_length(v8::Isolate * isolate, v8::Local<v8::Value> array_value);


std::vector<std::string> get_object_keys(v8::Isolate * isolate,
                                         v8::Local<v8::Object> & object,
                                         bool own_properties_only = true);


/**d
 * When passed a value representing an array, runs callable with each element of that array (but not on arrays
 *   contained within the outer array)
 * On any other object type, runs callable with that element
 */
template<class Callable>
void for_each_value(const v8::Local<v8::Context> context, const v8::Local<v8::Value> value, Callable callable) {

    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto length = get_array_length(context->GetIsolate(), array);
        for (int i = 0; i < length; i++) {
            callable(array->Get(context, i).ToLocalChecked());
        }
    } else {
        callable(value);
    }
}


/**
* Creates a variable with the given alias_name in the context's global object to point back to the global object
* Same as node.js "global" variable or a web browser "window" object
*/
void set_global_object_alias(v8::Isolate * isolate, const v8::Local<v8::Context> context, std::string alias_name);


/**
 * Returns a string corresponding to the type of the value passed
 * @param value the value whose type to return a string version of
 * @return a string for the type of value
 */
std::string get_type_string_for_value(v8::Local<v8::Value> value);


/**
* parses v8-related flags and removes them, adjusting argc as needed
*/
void process_v8_flags(int & argc, char ** argv);


/**
* exposes the garbage collector to javascript
* same as passing --expose-gc as a command-line flag
* To encourage the javascript garbage collector to run from c++ code, use: 
*   while(!v8::Isolate::IdleNotificationDeadline([time])) {};
*/  
void expose_gc();

// calls a javascript function with no parameters and returns the value
v8::Local<v8::Value> call_simple_javascript_function(v8::Isolate * isolate,
						     v8::Local<v8::Function> function);
/**
* Calls callable with each javascript "own property" in the object passed.
*/
template<class T>
void for_each_own_property(const v8::Local<v8::Context> context, const v8::Local<v8::Object> object, T callable)
{
    auto own_properties = object->GetOwnPropertyNames(context).ToLocalChecked();
    for_each_value(context, own_properties, [&object, &context, &callable](v8::Local<v8::Value> property_name){
        auto property_value = object->Get(context, property_name);
        
        callable(property_name, property_value.ToLocalChecked());
    });
}



/**
* Takes a container, runs each element through Callable and returns a new container
*   of the same container type but with a data type matching the returned type
*   of the Callable
*/
template<class Container,
         class Callable>
struct MapperHelper;


/**
* Takes a container containing type Data (only for single-type containers, not maps)
*   and runs each element through a Callable returning a new container of the same container type
*   but with a data type matching the type returned by the Callable
*/
template<template <typename, typename...> class Container,
class Data,
class... AddParams,
class Callable>
struct MapperHelper<Container<Data, AddParams...>, Callable>
{
    using Results = Container<typename std::result_of<Callable(Data)>::type>;
    Results operator()(const Container<Data, AddParams...> & container, Callable callable)
    {
        Results results;
        for (auto && element : container) {
            try {
                results.push_back(callable(std::forward<decltype(element)>(element)));
            }catch(...) {} // ignore exceptions, just don't copy the element
        }
        return results;
    }
};


/**
* Takes a map with arbitrary key/value types and returns a new map with the types
*   inside the std::pair returned by Callable
*/
template<class Key,
        class Value,
        class... AddParams,
        class Callable>
struct MapperHelper<std::map<Key, Value, AddParams...>, Callable>
{
    using Source = std::map<Key, Value, AddParams...>;
    using ResultPair = typename std::result_of<Callable(typename Source::value_type)>::type;
    using Results = std::map<typename ResultPair::T1, typename ResultPair::T2>;
    Results operator()(std::map<Key, Value, AddParams...> container, Callable callable)
    {
        Results results;


        for (auto && element : container) {
            results.insert(callable(std::forward<decltype(element)>(element)));
        }
        return results;
    }
};


/** IF YOU GET AN ERROR ABOUT RESULT_OF::TYPE NOT EXISTING, MAKE SURE YOUR LAMBDA PARAMETER TYPE IS EXACTLY RIGHT,
 * ESPECTIALLY RE; CONST
 * @param container input container
 * @param callable  transformation callback
 * @return container of the transformed results
 */
template <class Container, class Callable>
auto mapper(const Container & container, Callable callable) -> decltype(MapperHelper<Container, Callable>()(container, callable))
{
    return MapperHelper<Container, Callable>()(container, callable);
}


template <class Callable,
        class Container>
auto reducer(const Container & container, Callable callable) ->
    std::vector<typename std::result_of<Callable(typename Container::value_type)>::type>
{
    using ResultType = typename std::result_of<Callable(typename Container::value_type)>::type;
    std::vector<ResultType> results;
    for(auto && pair : container) {
        results.push_back(callable(std::forward<decltype(pair)>(pair)));
    }
    return results;
}







struct StuffBase{
    // virtual destructor makes sure derived class destructor is called to actually
    //   delete the data
    virtual ~StuffBase(){}
};

template<class T>
struct Stuff : public StuffBase {
    Stuff(T && t) : stuffed(std::make_unique<T>(std::move(t))) {}
    Stuff(std::unique_ptr<T> t) : stuffed(std::move(t)) {}


    static std::unique_ptr<Stuff<T>> stuffer(T && t) {return std::make_unique<Stuff<T>>(std::move(t));}
    static std::unique_ptr<Stuff<T>> stuffer(T const & t) {return std::make_unique<Stuff<T>>(std::move(const_cast<T &>(t)));}

    T * get(){return stuffed.get();}
    std::unique_ptr<T> stuffed;
};


template<class T>
auto get_value_as(v8::Isolate * isolate, v8::Local<v8::Value> value) {
    bool valid = false;
    if constexpr(std::is_same<T, v8::Function>::value) {
        if (value->IsFunction()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same<T, v8::Object>::value) {
        if (value->IsObject()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same<T, v8::Array>::value) {
        if (value->IsArray()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same<T, v8::String>::value) {
        if (value->IsString()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same<T, v8::Boolean>::value) {
        if (value->IsBoolean()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same<T, v8::Number>::value) {
        if(value->IsNumber()) {
            return v8::Local<T>::Cast(value);
        }
    } else if constexpr(std::is_same<T, v8::Value>::value) {
        // this can be handy for dealing with global values
        //   passed in through the version that takes globals
        return v8::Local<T>::Cast(value);

    } else {
        return CastToNative<T>()(isolate, value);
    }

    //printf("Throwing exception, failed while trying to cast value as type: %s\n", demangle<T>().c_str());
    //print_v8_value_details(value);
    throw v8toolkit::CastException(fmt::format("Couldn't cast value to requested type", demangle<T>().c_str()));

}


template<class T>
auto get_value_as(v8::Isolate * isolate, v8::Global<v8::Value> & value) {
    return get_value_as<T>(isolate, value.Get(isolate));
}





template<class T>
auto get_key_as(v8::Local<v8::Context> context, v8::Local<v8::Object> object, std::string const & key) {

    auto isolate = context->GetIsolate();
    // printf("Looking up key %s\n", key.c_str());
    auto get_maybe = object->Get(context, v8::String::NewFromUtf8(isolate, key.c_str()));

    if(get_maybe.IsEmpty() || get_maybe.ToLocalChecked()->IsUndefined()) {
//        if (get_maybe.IsEmpty()) {
//            std::cerr << "empty" << std::endl;
//        } else {
//            std::cerr << "undefined" << std::endl;
//        }
        throw UndefinedPropertyException(key);
    }
    return get_value_as<T>(isolate, get_maybe.ToLocalChecked());
}



template<class T>
auto get_key_as(v8::Local<v8::Context> context, v8::Local<v8::Value> object, std::string const & key) {
    return get_key_as<T>(context, get_value_as<v8::Object>(context->GetIsolate(), object), key);
}


template<class T>
auto get_key_as(v8::Local<v8::Context> context, v8::Global<v8::Value> & object, std::string const & key) {
    return get_key_as<T>(context, object.Get(context->GetIsolate()), key);
}

 v8::Local<v8::Value> get_key(v8::Local<v8::Context> context, v8::Local<v8::Object> object, std::string key);

 v8::Local<v8::Value> get_key(v8::Local<v8::Context> context, v8::Local<v8::Value> value, std::string key);


/**
* Takes a v8::Value and prints it out in a json-like form (but includes non-json types like function)
*
* Good for looking at the contents of a value and also used for printobj() method added by add_print
*/
std::string stringify_value(v8::Isolate * isolate,
                            const v8::Local<v8::Value> & value,
                            bool show_all_properties=false,
                            std::vector<v8::Local<v8::Value>> && processed_values = std::vector<v8::Local<v8::Value>>{});

/**
 * Tests if the given name conflicts with a reserved javascript top-level name
 * @param name value to check
 * @return true if there is a conflict
 */
bool global_name_conflicts(const std::string & name);
inline std::vector<std::string> reserved_global_names = {"Boolean", "Null", "Undefined", "Number", "String",
                                                  "Object", "Symbol", "Date", "Array", "Set", "WeakSet",
                                                  "Map", "WeakMap", "JSON"};


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




} // End v8toolkit namespace
