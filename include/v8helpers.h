#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <set>
#include <typeinfo>

#include <cppformat/format.h>


// Everything in here is standalone and does not require any other v8toolkit files
#include <libplatform/libplatform.h>
#include <v8.h>


namespace v8toolkit {

template <class... > struct TypeList {};

// This function is not defined and can only be called inside decltype()
template <class R, class... Ts>
auto get_typelist_for_function(std::function<R(Ts...)>) ->TypeList<Ts...>;


template <bool... b> struct static_all_of;

// If the first parameter is true, look at the rest of the list
template <bool... tail>
struct static_all_of<true, tail...> : static_all_of<tail...> {};

// if any parameter is false, return false
template <bool... tail>
struct static_all_of<false, tail...> : std::false_type {};

// If there are no parameters left, no false was found so return true
template <> struct static_all_of<> : std::true_type {};



#define TYPE_DETAILS(thing) fmt::format("const: {} type: {}", std::is_const<decltype(thing)>::value, typeid(thing).name()).c_str()

// thrown when data cannot be converted properly
class CastException : public std::exception {
private:
    std::string reason;

public:
    CastException(const std::string & reason) : reason(reason) {}
    virtual const char * what() const noexcept override {return reason.c_str();}
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
        auto array = v8::Object::Cast(*value);
        int i = 0;
        while(array->Has(context, i).FromMaybe(false)) {
            callable(array->Get(context, i).ToLocalChecked());
            i++;
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
* parses v8-related flags and removes them, adjusting argc as needed
*/
void process_v8_flags(int & argc, char ** argv);


/**
* exposes the garbage collector to javascript
* same as passing --expose-gc as a command-line flag
* To encourage javascript garbage collection run from c++, use: 
*   while(!v8::Isolate::IdleNotificationDeadline([time])) {};
*/  
void expose_gc();





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
struct AnyBase
{
    virtual ~AnyBase();
};


template<class T>
struct AnyPtr : public AnyBase {
    AnyPtr(T * data) : data(data) {}
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
    Any(T data) : data(data) {}
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
        printf("Tried to get value as %s\n", typeid(T).name());
        print_v8_value_details(value);
        printf("Throwing exception\n");
        throw "Bad Cast";
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
    if(get_maybe.IsEmpty()) {
        throw "no such key";
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
std::string stringify_value(v8::Isolate * isolate, const v8::Local<v8::Value> & value, bool toplevel=true, bool show_all_properties=false);






} // End v8toolkit namespace
