#include <string>
#include <map>
#include <vector>


// Everything in here is standalone and does not require any other v8toolkit files

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

namespace v8toolkit {


/**
* Returns the length of an array
*/
int get_array_length(v8::Isolate * isolate, v8::Local<v8::Array> array);
int get_array_length(v8::Isolate * isolate, v8::Local<v8::Value> array_value);


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
    auto operator()(const Container<Data, AddParams...> & container, Callable callable) -> Container<decltype(callable(std::declval<Data>()))>
    {
        Container<decltype(callable(std::declval<Data>())), AddParams...> results;
        for (auto element : container) {
            try {
                results.push_back(callable(element));
            }catch(...) {} // ignore exceptions, just don't copy the element
        }
        return results;
    }
};


/**
* Takes a map with arbitrary key/value types and returns a new map with the types
*   inside the std::pair returned by Callable
*/
template<
class Key,
class Value,
class... AddParams,
class Callable>
struct MapperHelper<std::map<Key, Value, AddParams...>, Callable>
{
    auto operator()(std::map<Key, Value, AddParams...> container, Callable callable) -> std::map<decltype(callable(std::declval<std::pair<Key, Value>>()).first), decltype(callable(std::declval<std::pair<Key, Value>>()).second), AddParams...>
    {
        std::map<decltype(callable(std::declval<std::pair<Key, Value>>()).first), decltype(callable(std::declval<std::pair<Key, Value>>()).second), AddParams...> results;
        for (auto element : container) {
            results.insert(callable(element));
        }
        return results;
    }
};


// simple map/transform method for a container supporting push_back
// TODO: Needs SFINAE
template <class Callable,
          template <typename, typename...> class Container,
          typename... ContainerParams >
auto mapper(const Container<ContainerParams...> & container, Callable callable) -> decltype(MapperHelper<Container<ContainerParams...>, Callable>()(container, callable))
{
    return MapperHelper<Container<ContainerParams...>, Callable>()(container, callable);
}


template <class Callable,
          template <typename, typename...> class Container,
typename Key,
          typename Value,
          typename... AddParams >
auto reducer(const Container<Key, Value, AddParams...> & container, Callable callable) -> std::vector<decltype(callable(std::declval<std::pair<Key, Value>>()))>
{
    std::vector<decltype(callable(std::declval<std::pair<Key, Value>>()))> results;
    for(auto & pair : container) {
        results.push_back(callable(pair));
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

















} // End v8toolkit namespace