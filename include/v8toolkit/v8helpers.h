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
#include <xl/demangle.h>

#include "log.h"
#include "type_traits.h"
#include "stdfunctionreplacement.h"
#include "cast_to_native.h"


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

template<typename T>
v8::Local<T> make_local(v8::Local<T> const & value) {
    return value;
}

template<typename T>
v8::Local<T> make_local(v8::Global<T> const & value) {
    auto isolate = v8::Isolate::GetCurrent();
    return value.Get(isolate);
}


/**
 * Returns a javascript string of the specified string in the GetCurrent() isolate
 */
v8::Local<v8::String> make_js_string(std::string_view str);


/**
 * Returns a std::string representing the string value for the value passed in
 * For string/symbol it is its own value, otherwise it is the to_string of the value
 */
std::string make_cpp_string(v8::Local<v8::Value> value);


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



class Exception : public std::exception {
protected:
    mutable std::string message;
    v8::Global<v8::StackTrace> stack_trace;
    mutable bool stack_trace_expanded = false;

public:
    template<class... Ts>
    Exception(std::string const & format, Ts&&... args) :
        message(fmt::format(format, args...)),
        stack_trace(v8::Global<v8::StackTrace>(v8::Isolate::GetCurrent(),
                                               v8::StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 100)))

    {}

    Exception(){}

    virtual const char * what() const noexcept override {
        if (!stack_trace_expanded) {
            stack_trace_expanded = true;
            message = fmt::format("{} - {}", message, get_stack_trace_string(make_local(stack_trace)));
        }
        return message.c_str();
    }
};



// thrown when data cannot be converted properly
class CastException : public Exception {

public:
    using Exception::Exception;

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



// polymorphic types work just like normal
template<class Destination, class Source, std::enable_if_t<std::is_polymorphic<Source>::value, int> = 0>
Destination safe_dynamic_cast(Source * source) {
    static_assert(std::is_pointer<Destination>::value, "must be a pointer type");
    static_assert(!std::is_pointer<std::remove_pointer_t<Destination>>::value, "must be a single pointer type");
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
auto make_tuple_for_variables(Ts&&... ts) {
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



/**
* General purpose exception for invalid uses of the v8toolkit API
*/
class InvalidCallException : public Exception {

public:
    InvalidCallException(const std::string & message);
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
int get_array_length(v8::Local<v8::Value> array_value);


std::vector<std::string> get_object_keys(v8::Isolate * isolate,
                                         v8::Local<v8::Object> object,
                                         bool own_properties_only = true);

std::vector<std::string> get_object_keys(v8::Local<v8::Object> object,
                                         bool own_properties_only = true);


/**d
 * When passed a value representing an array, runs callable with each element of that array (but not on arrays
 *   contained within the outer array)
 * On any other object type, runs callable with that element
 */
template<class Callable>
void for_each_value(v8::Local<v8::Value> const value, Callable callable) {

    auto isolate = v8::Isolate::GetCurrent();
    auto context = isolate->GetCurrentContext();
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
void set_global_object_alias(v8::Local<v8::Context> context, std::string const & alias_name);


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

inline v8::Local<v8::Value> call_simple_javascript_function(v8::Local<v8::Function> function) {
    return call_simple_javascript_function(v8::Isolate::GetCurrent(), function);
}


/**
* Calls callable with each javascript "own property" in the object passed.
*/
template<class T>
void for_each_own_property(const v8::Local<v8::Context> context, const v8::Local<v8::Object> object, T && callable)
{
    auto own_properties = object->GetOwnPropertyNames(context).ToLocalChecked();
    for_each_value(own_properties, [&object, &context, &callable](v8::Local<v8::Value> property_name){
        auto property_value = object->Get(context, property_name);
        
        std::forward<T>(callable)(property_name, property_value.ToLocalChecked());
    });
}

template<class T>
void for_each_own_property(const v8::Local<v8::Object> object, T && callable)
{
  return for_each_own_property(
      v8::Isolate::GetCurrent()->GetCurrentContext(), 
      object, 
      std::forward<T>(callable));
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


template<typename T, typename U>
std::optional<v8::Local<T>> get_value_as_optional(U && value_input) {
    auto value = make_local(value_input);

    if constexpr(std::is_same_v<T, v8::Function>) {
        if (value->IsFunction()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Object>) {
        if (value->IsObject()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Array>) {
        if (value->IsArray()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::String>) {
        if (value->IsString()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Boolean>) {
        if (value->IsBoolean()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Number>) {
        if(value->IsNumber()) {
            return v8::Local<T>::Cast(value);
        }
    } else if constexpr(std::is_same_v<T, v8::Value>) {
        // this can be handy for dealing with global values
        //   passed in through the version that takes globals
        return v8::Local<T>::Cast(value);
    } 
    return {};
};


/**
 * Returns the given JavaScript value as the parameterized type
 * @tparam T
 * @param isolate
 * @param value
 * @return
 */
template<class T, class U>
auto get_value_as(v8::Isolate * isolate, U && input_value) {

    auto value = make_local(input_value);
    
    if constexpr(std::is_same_v<T, v8::Function>) {
        if (value->IsFunction()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Object>) {
        if (value->IsObject()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Array>) {
        if (value->IsArray()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::String>) {
        if (value->IsString()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Boolean>) {
        if (value->IsBoolean()) {
            return v8::Local<T>::Cast(value);
        }

    } else if constexpr(std::is_same_v<T, v8::Number>) {
        if(value->IsNumber()) {
            return v8::Local<T>::Cast(value);
        }
    } else if constexpr(std::is_same_v<T, v8::Value>) {
        // this can be handy for dealing with global values
        //   passed in through the version that takes globals
        return v8::Local<T>::Cast(value);

    } else {
        return CastToNative<T>()(isolate, value);
    }

    //printf("Throwing exception, failed while trying to cast value as type: %s\n", demangle<T>().c_str());
    //print_v8_value_details(value);
    throw v8toolkit::CastException(fmt::format("Couldn't cast value to requested type: {}", xl::demangle<T>().c_str()));

}

template<typename T, typename U>
auto get_value_as(U && value) {
    auto isolate = v8::Isolate::GetCurrent();
    return get_value_as<T>(isolate, std::forward<U>(value));
}


template<typename T, typename U>
auto get_key_as(v8::Local<v8::Context> context, U && input, std::string_view key) {

    auto object = get_value_as<v8::Object>(input);
    
    auto isolate = context->GetIsolate();
    // printf("Looking up key %s\n", key.c_str());
    auto get_maybe = object->Get(context, v8::String::NewFromUtf8(isolate, 
                                                                  key.data(), 
                                                                  v8::String::NewStringType::kNormalString, 
                                                                  key.length()));

    if(get_maybe.IsEmpty() || get_maybe.ToLocalChecked()->IsUndefined()) {
//        if (get_maybe.IsEmpty()) {
//            std::cerr << "empty" << std::endl;
//        } else {
//            std::cerr << "undefined" << std::endl;
//        }
        throw UndefinedPropertyException(std::string(key));
    }
    return get_value_as<T>(isolate, get_maybe.ToLocalChecked());
}




template<class T>
auto get_key_as(v8::Local<v8::Value> object, std::string_view key) {
    auto isolate = v8::Isolate::GetCurrent();
    auto context = isolate->GetCurrentContext();
    return get_key_as<T>(context, get_value_as<v8::Object>(context->GetIsolate(), object), key);
}


template<class T>
auto get_key_as(v8::Local<v8::Context> context, v8::Global<v8::Value> & object, std::string_view key) {
    return get_key_as<T>(context, object.Get(context->GetIsolate()), key);
}


template<typename T>
v8::Local<v8::Value> get_key(T && object, std::string key) {
    return get_key_as<v8::Value>(std::forward<T>(object), key);
}


/**
 * This should eventually replace all the get_key_as functions
 */
template<typename Result=v8::Value, typename T>
std::optional<v8::Local<Result>> get_property_as(T && input_value, std::string_view key) {
    auto isolate = v8::Isolate::GetCurrent();
    auto context = isolate->GetCurrentContext();
    auto local_value = make_local(input_value);
    if (!local_value->IsObject()) {
        return {};
    }
    
    v8::Local<v8::Object> local_object = local_value->ToObject();

    // if it doesn't have the property at all, don't return undefined
    if (!local_object->HasOwnProperty(context, make_js_string(key)).FromMaybe(false)) {
        return {};
    }
    
    auto get_result = local_object->Get(context, make_js_string(key));
    if (get_result.IsEmpty()) {
        return {};
    }
    auto value = get_result.ToLocalChecked();
    return get_value_as_optional<Result>(value);
}


template<typename T>
std::optional<v8::Local<v8::Value>> get_property(T && object, std::string_view key) {
    return get_property_as<v8::Value>(object, key);
}


/**
* Takes a v8::Value and prints it out in a json-like form (but includes non-json types like function)
*
* Good for looking at the contents of a value and also used for printobj() method added by add_print
*/
std::string stringify_value(v8::Local<v8::Value> value,
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

bool is_reserved_word_in_static_context(std::string const & name);




} // End v8toolkit namespace
