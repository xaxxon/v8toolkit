#pragma once

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <set>
#include <typeinfo>
#include <fmt/ostream.h>

#include "stdfunctionreplacement.h"



// Everything in here is standalone and does not require any other v8toolkit files
#include <libplatform/libplatform.h>
#include <v8.h>

// if it can be determined safely that cxxabi.h is available, include it for name demangling
#if defined __has_include
#if __has_include(<cxxabi.h>)
#  define V8TOOLKIT_DEMANGLE_NAMES
#  include <cxxabi.h>
#endif
#endif


namespace v8toolkit {

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




    template<class T>
    using void_t = void;

    template<class T>
    using int_t = int;


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




// thrown when data cannot be converted properly
class CastException : public std::exception {
private:
    std::string reason;

public:
    CastException(const std::string & reason) : reason(reason + get_stack_trace_string(v8::StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 100))) {}
    virtual const char * what() const noexcept override {return reason.c_str();}
};

template<typename T, typename = void>
struct ProxyType {
    using PROXY_TYPE = T;
};

template<typename T>
struct ProxyType<T,void_t<typename T::V8TOOLKIT_PROXY_TYPE>>{
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
std::string demangle(){
    auto demangled_name =  demangle_typeid_name(typeid(T).name());
    std::string constness = std::is_const<T>::value ? "const " : "";
    std::string volatility = std::is_volatile<T>::value ? "volatile " : "";

    return constness + volatility + demangled_name;
 }

template<class Destination, class Source, std::enable_if_t<std::is_polymorphic<Source>::value, int> = 0>
Destination safe_dynamic_cast(Source * source) {
    static_assert(std::is_pointer<Destination>::value, "must be a pointer type");
    static_assert(!std::is_pointer<std::remove_pointer_t<Destination>>::value, "must be a single pointer type");
//    fprintf(stderr, "safe dynamic cast doing real cast\n");
    return dynamic_cast<Destination>(source);
};
template<class Destination, class Source, std::enable_if_t<!std::is_polymorphic<Source>::value, int> = 0>
Destination safe_dynamic_cast(Source * source) {
    static_assert(std::is_pointer<Destination>::value, "must be a pointer type");
    static_assert(!std::is_pointer<std::remove_pointer_t<Destination>>::value, "must be a single pointer type");
//    fprintf(stderr, "safe dynamic cast doing fake/stub cast\n");
    return nullptr;
};


#define SAFE_MOVE_CONSTRUCTOR_SFINAE !std::is_const<T>::value && std::is_move_constructible<T>::value
template<class T, std::enable_if_t<SAFE_MOVE_CONSTRUCTOR_SFINAE, int> = 0>
std::unique_ptr<T> safe_move_constructor(T && original) {
    return std::make_unique<T>(std::move(original));
};
template<class T, std::enable_if_t<!(SAFE_MOVE_CONSTRUCTOR_SFINAE), int> = 0>
std::unique_ptr<T> safe_move_constructor(T && original) {
    assert(false); // This shouldn't be called
    return std::unique_ptr<T>();
};



    template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {};

template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref>: std::true_type {};

/**
 * Returns a func::function type compatible with the lambda passed in
 */
template<class T>
struct LTG {
    template<class R, class... Args>
    static auto go(R(T::*)(Args...)const)->func::function<R(Args...)>;

    template<class R, class... Args>
    static auto go(R(T::*)(Args...))->func::function<R(Args...)>;

    template<class R, class... Args>
    static auto go(R(T::*)(Args...)const &)->func::function<R(Args...)>;

    template<class R, class... Args>
    static auto go(R(T::*)(Args...) &)->func::function<R(Args...)>;

};

template<class T>
struct LTG<T &&> {
    template<class R, class... Args>
    static auto go(R(T::*)(Args...)const &&)->func::function<R(Args...)>;

    template<class R, class... Args>
    static auto go(R(T::*)(Args...) &&)->func::function<R(Args...)>;

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



template <bool... b> struct static_all_of;

// If the first parameter is true, look at the rest of the list
template <bool... tail>
struct static_all_of<true, tail...> : static_all_of<tail...> {};

// if any parameter is false, return false
template <bool... tail>
struct static_all_of<false, tail...> : std::false_type {};

// If there are no parameters left, no false was found so return true
template <> struct static_all_of<> : std::true_type {};



#define TYPE_DETAILS(thing) fmt::format("const: {} type: {}", std::is_const<decltype(thing)>::value, demangle<decltype(thing)>()).c_str()

/**
* General purpose exception for invalid uses of the v8toolkit API
*/
class InvalidCallException : public std::exception {
private:
    std::string message;

public:
    InvalidCallException(const std::string & message);
    virtual const char * what() const noexcept override {return message.c_str();}
};

/**
 * Thrown when trying to register a function/method/member with the same name as
 * something else already registered
 */
class DuplicateNameException : public std::exception {
private:
    std::string message;

public:
    DuplicateNameException(const std::string & message) : message(message) {}
    virtual const char * what() const noexcept override {return message.c_str();}
};


 class UndefinedPropertyException : public std::exception {
 private:
     std::string message;

 public:
 UndefinedPropertyException(const std::string & message) : message(message) {}
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


std::set<std::string> make_set_from_object_keys(v8::Isolate * isolate,
                                                v8::Local<v8::Object> & object,
                                                bool own_properties_only = true);


    /**
* When passed a value representing an array, runs callable with each element of that array (but not on arrays 
*   contained within the outer array)
* On any other object type, runs callable with that element
*/
template<class Callable>
void for_each_value(const v8::Local<v8::Context> context, const v8::Local<v8::Value> value, Callable callable) {
    
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
	auto length = get_array_length(context->GetIsolate(), array);
	for(int i = 0; i < length; i++) {
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
void expose_debug(const std::string & debug_name);

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




/**
* When passing an Any-type through a void *, always static_cast it to an AnyBase *
*   pointer and pass that as the void *.  This allows you to safely cast it back to
*   a AnyBase* on the other side and then dynamic_cast to any child types to 
*   determine the type of the object actually stored.
*/

// if this is defined, AnyBase will store the actual typename but this is only needed for debugging
//#define ANYBASE_DEBUG


 struct AnyBase
{
    virtual ~AnyBase();
#ifdef ANYBASE_DEBUG
    std::string type_name;
#endif
AnyBase(const std::string &&
#ifdef ANYBASE_DEBUG
	     type_name
#endif
	    )
#ifdef ANYBASE_DEBUG
: type_name(std::move(type_name))
#endif
    {}
};

 template<class T, class = void>
 struct AnyPtr;


 template<class T>
     struct AnyPtr<T, std::enable_if_t<!std::is_pointer<T>::value && !std::is_reference<T>::value>> : public AnyBase {
 AnyPtr(T * data) : AnyBase(demangle<T>()), data(data) {}
    virtual ~AnyPtr(){}
    T* data;
    T * get() {return data;}
};

/**
* Best used for types that are intrinsically pointers like std::shared_ptr or
*   std::exception_ptr
*/
template<class T>
struct Any : public AnyBase {
 Any(T data) : AnyBase(demangle<T>()), data(data) {}
    virtual ~Any(){}
    T data;
    T get() {return data;}
};


template<class T>
v8::Local<T> get_value_as(v8::Local<v8::Value> value) {
    bool valid = false;
    if (std::is_same<T, v8::Function>::value) {
        valid = value->IsFunction();
    } else if (std::is_same<T, v8::Object>::value) {
        valid = value->IsObject();
    } else if (std::is_same<T, v8::Array>::value) {
        valid = value->IsArray();
    } else if (std::is_same<T, v8::String>::value) {
        valid = value->IsString();
    } else if (std::is_same<T, v8::Boolean>::value) {
        valid = value->IsBoolean();
    } else if (std::is_same<T, v8::Number>::value) {
        valid = value->IsNumber();
    } else if (std::is_same<T, v8::Value>::value) {
        // this can be handy for dealing with global values
        //   passed in through the version that takes globals
        valid = true;
    }

    if (valid){
        return v8::Local<T>::Cast(value);
    } else {
        //printf("Throwing exception, failed while trying to cast value as type: %s\n", demangle<T>().c_str());
        //print_v8_value_details(value);
	    throw v8toolkit::CastException(fmt::format("Couldn't cast value to requested type", demangle<T>().c_str()));
    }
}


template<class T>
v8::Local<T> get_value_as(v8::Isolate * isolate, v8::Global<v8::Value> & value) {
    return get_value_as<T>(value.Get(isolate));
}




template<class T>
v8::Local<T> get_key_as(v8::Local<v8::Context> context, v8::Local<v8::Object> object, std::string key) {

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
    return get_value_as<T>(get_maybe.ToLocalChecked());
}



template<class T>
v8::Local<T> get_key_as(v8::Local<v8::Context> context, v8::Local<v8::Value> object, std::string key) {
    return get_key_as<T>(context, get_value_as<v8::Object>(object), key);
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
extern std::vector<std::string> reserved_global_names;

} // End v8toolkit namespace
