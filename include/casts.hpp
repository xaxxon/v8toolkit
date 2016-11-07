#ifndef CASTS_HPP
#define CASTS_HPP

#include <assert.h>

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <deque>
#include <array>
#include <memory>
#include <utility>
#include "v8.h"

#include "v8helpers.h"

namespace v8toolkit {

    // if a value to send to a macro has a comma in it, use this instead so it is parsed as a comma character in the value
    //   and not separating another parameter to the template
#define V8TOOLKIT_COMMA ,


    // add inside CastToNative::operator() to have it handle
    //   with no parameters
#define HANDLE_FUNCTION_VALUES \
    { \
	if (value->IsFunction()) { \
	    value = v8toolkit::call_simple_javascript_function(isolate, v8::Local<v8::Function>::Cast(value)); \
	} \
    }


// Use this macro when you need to customize the template options to something more than <class T>
//   For instance if you need to make it <vector<T>>
#define CAST_TO_NATIVE_WITH_CONST(TYPE, TEMPLATE) \
template<TEMPLATE> \
 struct v8toolkit::CastToNative<TYPE>{				\
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
        return v8toolkit::CastToNative<const TYPE>()(isolate, value);	\
    } \
}; \
\
template<TEMPLATE> \
 struct v8toolkit::CastToNative<TYPE const> {				\
    TYPE const operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
	HANDLE_FUNCTION_VALUES;


#define CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(TYPE) \
template<> \
 struct v8toolkit::CastToNative<TYPE> {				\
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const; \
}; \
\
template<> \
 struct v8toolkit::CastToNative<const TYPE>{				\
    const TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
        return v8toolkit::CastToNative<TYPE>()(isolate, value);	\
    } \
}; \
 inline TYPE v8toolkit::CastToNative<TYPE>::operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
    HANDLE_FUNCTION_VALUES;



#define CAST_TO_JS_TEMPLATED(TYPE, TEMPLATE) \
template<TEMPLATE> struct v8toolkit::CastToJS<const TYPE> {		\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const & value) const; \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const && value) const { return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
template<TEMPLATE> \
struct v8toolkit::CastToJS<TYPE> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE & value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE && value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
template<TEMPLATE> \
struct v8toolkit::CastToJS<TYPE &> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE & value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
template<TEMPLATE> \
struct v8toolkit::CastToJS<const TYPE &> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE const & value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
template<TEMPLATE> \
inline v8::Local<v8::Value>  v8toolkit::CastToJS<const TYPE>::operator()(v8::Isolate * isolate, TYPE const & value) const



#define CAST_TO_JS(TYPE)					\
template<> struct v8toolkit::CastToJS<const TYPE> {		\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const TYPE value) const; \
}; \
template<> \
 struct v8toolkit::CastToJS<TYPE> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
template<> \
 struct v8toolkit::CastToJS<TYPE &> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
 template<> \
 struct v8toolkit::CastToJS<const TYPE &> {					\
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE value) const {return v8toolkit::CastToJS<const TYPE>()(isolate, value);} \
}; \
inline v8::Local<v8::Value>  v8toolkit::CastToJS<const TYPE>::operator()(v8::Isolate * isolate, const TYPE value) const


/**
* Casts from a boxed Javascript type to a native type
*/
template<typename T, class = void>
struct CastToNative;


template<>
struct CastToNative<void> {
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {}
};


CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(bool)return static_cast<bool>(value->ToBoolean()->Value());}



// integers
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long long)return static_cast<long long>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned long long)return static_cast<unsigned long long>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long)return static_cast<long>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned long)return static_cast<unsigned long>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(int) return static_cast<int>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned int)return static_cast<unsigned int>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(short)return static_cast<short>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned short)return static_cast<unsigned short>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char)return static_cast<char>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned char)return static_cast<unsigned char>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(wchar_t)return static_cast<wchar_t>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char16_t)return static_cast<char16_t>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char32_t)return static_cast<char32_t>(value->ToInteger()->Value());}


template<class Return, class... Params>
struct CastToNative<std::function<Return(Params...)>> {
    std::function<Return(Params...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const;
};



    template<template<class,class> class ContainerTemplate, class SecondT, class FirstT>
    ContainerTemplate<FirstT, SecondT> pair_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {
        HANDLE_FUNCTION_VALUES;
        if (value->IsArray()) {
            auto length = get_array_length(isolate, value);
            if (length != 2) {
                auto error = fmt::format("Array to std::pair must be length 2, but was {}", length);
                isolate->ThrowException(v8::String::NewFromUtf8(isolate, error.c_str()));
                throw v8toolkit::CastException(error);
            }
            auto context = isolate->GetCurrentContext();
            auto array = get_value_as<v8::Array>(value);
            auto first = array->Get(context, 0).ToLocalChecked();
            auto second = array->Get(context, 1).ToLocalChecked();
            return std::pair<FirstT, SecondT>(v8toolkit::CastToNative<FirstT>()(isolate, first),
                                              v8toolkit::CastToNative<SecondT>()(isolate, second));

        } else {
            auto error = fmt::format("CastToNative<std::pair<T>> requires an array but instead got %s\n", stringify_value(isolate, value));
            std::cout << error << std::endl;
            throw v8toolkit::CastException(error);
        }
    }

template<class FirstT, class SecondT>
struct v8toolkit::CastToNative<std::pair<FirstT, SecondT>>{
    std::pair<FirstT, SecondT> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        pair_type_helper<std::pair, FirstT, SecondT>(isolate, value);
    }
};

template<class FirstT, class SecondT>
 struct v8toolkit::CastToNative<std::pair<FirstT, SecondT> const> {
     std::pair<FirstT, SecondT> const operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
	HANDLE_FUNCTION_VALUES;
    if (value->IsArray()) {
        auto length = get_array_length(isolate, value);
        if (length != 2) {
            auto error = fmt::format("Array to std::pair must be length 2, but was {}", length);
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, error.c_str()));
            throw v8toolkit::CastException(error);
        }
        auto context = isolate->GetCurrentContext();
        auto array = get_value_as<v8::Array>(value);
        auto first = array->Get(context, 0).ToLocalChecked();
        auto second = array->Get(context, 1).ToLocalChecked();
        return std::pair<FirstT, SecondT>(v8toolkit::CastToNative<FirstT>()(isolate, first),
                                          v8toolkit::CastToNative<SecondT>()(isolate, second));

    } else {
	auto error = fmt::format("CastToNative<std::pair<T>> requires an array but instead got %s\n", stringify_value(isolate, value));
	std::cout << error << std::endl;
        throw v8toolkit::CastException(error);
    }
}};




CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(float) return static_cast<float>(value->ToNumber()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(double) return static_cast<double>(value->ToNumber()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long double) return static_cast<long double>(value->ToNumber()->Value());}



template<>
struct CastToNative<v8::Local<v8::Function>> {
	v8::Local<v8::Function> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        if(value->IsFunction()) {
            return v8::Local<v8::Function>::Cast(value);
        } else {
            throw CastException(fmt::format(
                    "CastToNative<v8::Local<v8::Function>> requires a javascript function but instead got '{}'",
                    stringify_value(isolate, value)));
        }
    }
};

/**
 * char * and const char * are the only types that don't actually return their own type.  Since a buffer is needed
 *   to store the string, a std::unique_ptr<char[]> is returned.
 */
template<>
struct CastToNative<char *> {
  std::unique_ptr<char[]> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
      HANDLE_FUNCTION_VALUES;
    return std::unique_ptr<char[]>(strdup(*v8::String::Utf8Value(value)));
  }
};
template<>
struct CastToNative<const char *> {
  std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
      HANDLE_FUNCTION_VALUES;
      return CastToNative<char *>()(isolate, value);
  }
};

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(std::string)
    return std::string(*v8::String::Utf8Value(value));
}


template<template<class,class...> class VectorTemplate, class T, class... Rest>
auto vector_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) ->
    VectorTemplate<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>
{
    static_assert(!std::is_reference<T>::value, "vector value type cannot be reference");
    using ValueType = std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>;
    static_assert(!std::is_reference<ValueType>::value, "vector value type cannot be reference");
    using ResultType = VectorTemplate<ValueType, Rest...>;
    HANDLE_FUNCTION_VALUES;
    auto context = isolate->GetCurrentContext();
    ResultType v;
    if (value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for (int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            v.emplace_back(CastToNative<T>()(isolate, value));
        }
    } else {
        throw CastException(fmt::format("CastToNative<std::vector<{}>> requires an array but instead got JS: '{}'",
                                        demangle<T>(),
                                        stringify_value(isolate, value)));
    }
    return v;

}


//Returns a vector of the requested type unless CastToNative on ElementType returns a different type, such as for char*, const char *
template<class T, class... Rest>
struct CastToNative<std::vector<T, Rest...>> {
    using ResultType = std::vector<std::remove_reference_t<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>>, Rest...>;

    ResultType operator()(v8::Isolate *isolate, v8::Local<v8::Value> value) const {
        return vector_type_helper<std::vector, T, Rest...>(isolate, value);
    }
};

//Returns a vector of the requested type unless CastToNative on ElementType returns a different type, such as for char*, const char *
template<class T, class... Rest>
struct CastToNative<const std::vector<T, Rest...>> {

    using NonConstResultType = std::vector<std::result_of_t<CastToNative<T>(v8::Isolate *, v8::Local<v8::Value>)>, Rest...>;

    const NonConstResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return vector_type_helper<std::vector, T, Rest...>(isolate, value);
    }
};




template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<std::is_copy_constructible<T>::value>> {
    std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
	HANDLE_FUNCTION_VALUES;

	// if it's an object, use the memory in the object
        if (value->IsObject()) {
            auto object = value->ToObject();
            if (object->InternalFieldCount() == 1) {
                auto && result = CastToNative<T>()(isolate, value);
                return std::unique_ptr<T, Rest...>(CastToNative<T*>()(isolate, value));
            }
        }
	// otherwise, make a new instance of the type to store in the unique ptr
        return std::unique_ptr<T, Rest...>(new T(CastToNative<T>()(isolate, value)));
    }
};

template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<!std::is_copy_constructible<T>::value>> {
    std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
	HANDLE_FUNCTION_VALUES;
        // if T is a user-defined type
        if (value->IsObject()) {
            auto object = value->ToObject();
            if (object->InternalFieldCount() == 1) {
                auto && result = CastToNative<T>()(isolate, value);
                return std::unique_ptr<T, Rest...>(CastToNative<T*>()(isolate, value));
            }
        }
        throw CastException(fmt::format("Cannot make unique ptr for type {}  that is not wrapped and not copy constructible", demangle<T>()));
    }
};

template<template<class,class,class...> class ContainerTemplate, class Key, class Value, class... Rest>
ContainerTemplate<Key, Value, Rest...> map_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    //    MapType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
    if (!value->IsObject()) {
        throw CastException(
                fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                            stringify_value(isolate, value)));
    }

    auto context = isolate->GetCurrentContext();

    ContainerTemplate<Key, Value, Rest...> results;
    for_each_own_property(context, value->ToObject(),
                          [isolate, &results](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              results.emplace(v8toolkit::CastToNative<Key>()(isolate, key),
                                              v8toolkit::CastToNative<Value>()(isolate, value));
                          });
    return results;
}

template<class Key, class Value, class... Args>
struct CastToNative<std::map<Key, Value, Args...>> {
    std::map<Key, Value, Args...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return map_type_helper<std::map, Key, Value, Args...>(isolate, value);
    }
};

template<template<class,class,class...> class ContainerTemplate, class Key, class Value, class... Rest>
ContainerTemplate<Key, Value, Rest...> multimap_type_helper(v8::Isolate * isolate, v8::Local<v8::Value> value) {

    if (!value->IsObject()) {
        throw CastException(
                fmt::format("Javascript Object type must be passed in to convert to std::map - instead got {}",
                            stringify_value(isolate, value)));
    }

    auto context = isolate->GetCurrentContext();

    ContainerTemplate<Key, Value, Rest...> results;
    for_each_own_property(context, value->ToObject(),
                          [&](v8::Local<v8::Value> key, v8::Local<v8::Value> value) {
                              v8toolkit::for_each_value(context, value, [&](v8::Local<v8::Value> sub_value){
                                  results.emplace(v8toolkit::CastToNative<Key>()(isolate, key),
                                                  v8toolkit::CastToNative<Value>()(isolate, sub_value));
                              });
                          });
    return results;
}



template<class Key, class Value, class... Args>
struct CastToNative<std::multimap<Key, Value, Args...>> {

    using ResultType = std::multimap<Key, Value, Args...>;

    ResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        return multimap_type_helper<std::multimap, Key, Value, Args...>(isolate, value);
    }
};


//Returns a vector of the requested type unless CastToNative on ElementType returns a different type, such as for char*, const char *
template<class Key, class Value, class... Args>
struct CastToNative<std::map<Key, Value, Args...> const> {

    using NonConstResultType = std::map<std::result_of_t<CastToNative<Key>(v8::Isolate *, v8::Local<v8::Value>)>,
        std::result_of_t<CastToNative<Value>(v8::Isolate *, v8::Local<v8::Value>)>>;

    const NonConstResultType operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        // HANDLE_FUNCTION_VALUES; -- no need for this here, since we call another CastToNative from here
        return CastToNative<NonConstResultType>()(isolate, value);
    }
};


//


    /**
* Casts from a native type to a boxed Javascript type
*/

template<typename T, class = void>
struct CastToJS;

CAST_TO_JS(bool){return v8::Boolean::New(isolate, value);}

//TODO: Should all these operator()'s be const?
// integers
CAST_TO_JS(char){return v8::Integer::New(isolate, value);}
CAST_TO_JS(unsigned char){return v8::Integer::New(isolate, value);}

CAST_TO_JS(wchar_t){return v8::Number::New(isolate, value);}
CAST_TO_JS(char16_t){return v8::Integer::New(isolate, value);}
CAST_TO_JS(char32_t){return v8::Integer::New(isolate, value);}
CAST_TO_JS(short){return v8::Integer::New(isolate, value);}
CAST_TO_JS(unsigned short){return v8::Integer::New(isolate, value);}



CAST_TO_JS(int){return v8::Number::New(isolate, value);}

CAST_TO_JS(unsigned int){return v8::Number::New(isolate, value);}
CAST_TO_JS(long){return v8::Number::New(isolate, value);}

CAST_TO_JS(unsigned long){return v8::Number::New(isolate, value);}
CAST_TO_JS(long long){return v8::Number::New(isolate, static_cast<double>(value));}
CAST_TO_JS(unsigned long long){return v8::Number::New(isolate, static_cast<double>(value));}



// floats
CAST_TO_JS(float){return v8::Number::New(isolate, value);}
CAST_TO_JS(double){return v8::Number::New(isolate, value);}
CAST_TO_JS(long double){return v8::Number::New(isolate, value);}


CAST_TO_JS(std::string){return v8::String::NewFromUtf8(isolate, value.c_str());}

CAST_TO_JS(char *){return v8::String::NewFromUtf8(isolate, value);}

template<class T>
struct CastToJS<T**> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const T** multi_pointer) {
        return CastToJS<T*>(isolate, *multi_pointer);
    }
};



/**
* Special passthrough type for objects that want to take javascript object objects directly
*/
template<>
struct CastToJS<v8::Local<v8::Object>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Local<v8::Object> object){
		//return v8::Local<v8::Value>::New(isolate, object);
        return object;
	}
};



/**
* Special passthrough type for objects that want to take javascript value objects directly
*/
template<>
struct CastToJS<v8::Local<v8::Value>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value){
		return value;
	}
};

template<>
struct CastToJS<v8::Global<v8::Value> &> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Global<v8::Value> & value) {
        return value.Get(isolate);
    }
};




// CastToJS<std::pair<>>
template<class T1, class T2>
struct CastToJS<std::pair<T1, T2>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::pair<T1, T2> const & pair);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::pair<T1, T2> && pair) {
        return this->operator()(isolate, pair);
    }
};
template<class T1, class T2>
struct CastToJS<std::pair<T1, T2> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::pair<T1, T2> const & pair);
};

// CastToJS<std::vector<>>
template<class T, class... Rest>
struct CastToJS<std::vector<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::vector<T, Rest...> const & vector);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::vector<T, Rest...> && vector) {
        return this->operator()(isolate, vector);
    }
};
template<class T, class... Rest>
struct CastToJS<std::vector<T, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::vector<T, Rest...> const & vector);
};

// CastToJS<std::list>
template<class U, class... Rest>
struct CastToJS<std::list<U, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::list<U, Rest...> & list);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::list<U, Rest...> && list) {
        return this->operator()(isolate, list);
    }
};
template<class U, class... Rest>
struct CastToJS<std::list<U, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::list<U, Rest...> const & list);
};

// CastToJS<std::map>
template<class A, class B, class... Rest>
struct CastToJS<std::map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::map<A, B, Rest...> & map);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::map<A, B, Rest...> && map) {
        return this->operator()(isolate, map);
    }
};
template<class A, class B, class... Rest>
struct CastToJS<std::map<A, B, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::map<A, B, Rest...> const & map);
};

// CastToJS<std::multimap>
template<class A, class B, class... Rest>
struct CastToJS<std::multimap<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::multimap<A, B, Rest...> & multimap);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::multimap<A, B, Rest...> && multimap) {
        return this->operator()(isolate, multimap);
    }
};
template<class A, class B, class... Rest>
struct CastToJS<std::multimap<A, B, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::multimap<A, B, Rest...> const & multimap);
};


// CastToJS<std::undordered:map>
template<class A, class B, class... Rest>
struct CastToJS<std::unordered_map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unordered_map<A, B, Rest...> & unorderedmap);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unordered_map<A, B, Rest...> && unorderedmap) {
        return this->operator()(isolate, unorderedmap);
    }
};
template<class A, class B, class... Rest>
struct CastToJS<std::unordered_map<A, B, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unordered_map<A, B, Rest...> const & constunorderedmap);
};

// CastToJS<std::deque>
template<class T, class... Rest>
struct CastToJS<std::deque<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::deque<T, Rest...> & deque);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::deque<T, Rest...> && deque) {
        return this->operator()(isolate, deque);
    }
};
template<class T, class... Rest>
struct CastToJS<std::deque<T, Rest...> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::deque<T, Rest...> const & deque);
};

// CastToJS<std::array>
template<class T, std::size_t N>
struct CastToJS<std::array<T, N>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::array<T, N> & arr);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::array<T, N> && arr) {
        return this->operator()(isolate, arr);
    }
};
template<class T, std::size_t N>
struct CastToJS<std::array<T, N> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::array<T, N> const & arr);
};



/**
 * This is for when a function returns a std::unique - meaning it likely allocated new memory on its own
 * If this is being sent back to JS, the unique_ptr must release the memory, because the unique_ptr is going to
 * go out of scope immediately
 *
 * These functions are not const because they call unique_ptr::release
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> & unique_ptr);
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> && unique_ptr);
};


/**
 * If a data structure contains a unique_ptr and that is being returned, the unique_ptr should not ::release()
 * its memory.  This is treated just as if the call were returning a T* instead of a unique_ptr<T>
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> &> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> const & unique_ptr);
};
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...> const &> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> const & unique_ptr);
};



// CastToJS<std::shared>
template<class T>
struct CastToJS<std::shared_ptr<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::shared_ptr<T> & shared_ptr);
};
template<class T>
struct CastToJS<std::shared_ptr<T> const> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::shared_ptr<T> const & shared_ptr);
};


template<template<class...> class VectorTemplate, class ValueType, class... Rest>
v8::Local<v8::Value> cast_to_js_vector_helper(v8::Isolate * isolate, VectorTemplate<ValueType, Rest...> const & vector);


template<class T, class... Rest>
v8::Local<v8::Value>CastToJS<std::vector<T, Rest...>>::operator()(v8::Isolate *isolate, std::vector<T, Rest...> const & vector) {
    return cast_to_js_vector_helper<std::vector, T, Rest...>(isolate, vector);
}


template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::vector<T, Rest...> const>::operator()(v8::Isolate * isolate, std::vector<T, Rest...> const & vector){
    return CastToJS<std::vector<T, Rest...>>()(isolate, vector);
}



/**
* supports lists containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::list<T, Rest...>>::operator()(v8::Isolate * isolate, std::list<T, Rest...> & list) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    int i = 0;
    for (auto & element : list) {
        (void)array->Set(context, i, CastToJS<T&>()(isolate, element));
        i++;
    }
    return array;
}



template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::list<T, Rest...> const>::operator()(v8::Isolate * isolate, std::list<T, Rest...> const & list){
    return CastToJS<std::list<T, Rest...>>()(isolate, list);
}



/**
* supports maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::map<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::map<A, B, Rest...> & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto & pair : map){
    // Don't std::forward key/value values because they should never be std::move'd
    // CastToJS as reference type because the storage is allocated - it shouldn't be treated like a temporary
        (void)object->Set(context, CastToJS<A&>()(isolate, const_cast<A&>(pair.first)), CastToJS<B&>()(isolate, const_cast<B&>(pair.second)));
    }
    return object;
}


template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<const std::map<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::map<A, B, Rest...> const & map){
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto & pair : map){
        // Don't std::forward key/value values because they should never be std::move'd
        // CastToJS as reference type because the storage is allocated - it shouldn't be treated like a temporary
        (void)object->Set(context, CastToJS<A&>()(isolate, const_cast<A const &>(pair.first)), CastToJS<B const &>()(isolate, const_cast<B&>(pair.second)));
    }
    return object;
}


template<template<class,class,class...> class MultiMapLike, class A, class B, class... Rest>
v8::Local<v8::Object> casttojs_multimaplike(v8::Isolate * isolate, MultiMapLike<A, B, Rest...> const & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto & pair : map){
        auto key = CastToJS<A&>()(isolate, const_cast<A&>(pair.first));
        auto value = CastToJS<B&>()(isolate, const_cast<B&>(pair.second));

        // check to see if a value with this key has already been added
        bool default_value = true;
        bool object_has_key = object->Has(context, key).FromMaybe(default_value);
        if(!object_has_key) {
            // get the existing array, add this value to the end
            auto array = v8::Array::New(isolate);
            (void)array->Set(context, 0, value);
            (void)object->Set(context, key, array);
        } else {
            // create an array, add the current value to it, then add it to the object
            auto existing_array_value = object->Get(context, key).ToLocalChecked();
            v8::Handle<v8::Array> existing_array = v8::Handle<v8::Array>::Cast(existing_array_value);

            //find next array position to insert into (is there no better way to push onto the end of an array?)
            int i = 0;
            while(existing_array->Has(context, i).FromMaybe(default_value)){i++;}
            (void)existing_array->Set(context, i, value);
        }
    }
    return object;
}

/**
* supports maps containing any type also supported by CastToJS to javascript arrays
* It creates an object of key => [values...]
* All values are arrays, even if there is only one value in the array.
*/
template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::multimap<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> & map){
    return casttojs_multimaplike(isolate, map);
}



template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::multimap<A, B, Rest...> const>::operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> const & multimap){
    return CastToJS<std::multimap<A, B, Rest...>>()(isolate, multimap);
}



template<class T1, class T2> v8::Local<v8::Value>
CastToJS<std::pair<T1, T2>>::operator()(v8::Isolate *isolate, std::pair<T1, T2> const & pair) {

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    (void) array->Set(context, 0, CastToJS<T1 &>()(isolate, pair.first));
    (void) array->Set(context, 1, CastToJS<T2 &>()(isolate, pair.second));
    return array;
}



template<class T1, class T2> v8::Local<v8::Value>
CastToJS<std::pair<T1, T2> const>::operator()(v8::Isolate * isolate, std::pair<T1, T2> const & pair){
    return CastToJS<std::pair<T1, T2>>()(isolate, pair);
}

template<int position, class T>
struct CastTupleToJS;

template<class... Args>
struct CastTupleToJS<0, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - 0 - 1;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        using TuplePositionType = typename std::tuple_element<array_position, std::tuple<Args...>>::type;

        (void)array->Set(context,
                         array_position,
                         CastToJS<TuplePositionType &>()(isolate,
                                                       std::get<array_position>(tuple)));
        return array;
    }
};

template<int position, class... Args>
struct CastTupleToJS<position, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - position - 1;
        using TuplePositionType = typename std::tuple_element<array_position, std::tuple<Args...>>::type;

        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = CastTupleToJS<position - 1, std::tuple<Args...>>()(isolate, tuple);
        (void)array->Set(context,
                         array_position,
                         CastToJS<TuplePositionType &>()(isolate,
                                                       std::get<array_position>(tuple)));
        return array;
    }
};



template<class... Args>
struct CastToJS<std::tuple<Args...>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::tuple<Args...> tuple) {
        return CastTupleToJS<sizeof...(Args) - 1, std::tuple<Args...>>()(isolate, tuple);
    }
};


/**
* supports unordered_maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::unordered_map<A, B, Rest...>>::operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> & map){
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for(auto pair : map){
        (void)object->Set(context, CastToJS<A&>()(isolate, pair.first), CastToJS<B&>()(isolate, pair.second));
    }
    return object;
}

template<class A, class B, class... Rest> v8::Local<v8::Value>
CastToJS<std::unordered_map<A, B, Rest...> const>::operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> const & map) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto object = v8::Object::New(isolate);
    for (auto pair : map) {
        (void) object->Set(context, CastToJS<A &>()(isolate, pair.first), CastToJS<B &>()(isolate, pair.second));
    }
    return object;
}



/**
* supports deques containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::deque<T, Rest...>>::operator()(v8::Isolate * isolate, std::deque<T, Rest...> & deque) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    auto size = deque.size();
    for(unsigned int i = 0; i < size; i++) {
        (void)array->Set(context, i, CastToJS<T&>()(isolate, deque.at(i)));
    }
    return array;
}



template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::deque<T, Rest...> const>::operator()(v8::Isolate * isolate, std::deque<T, Rest...> const & deque){
    return CastToJS<std::deque<T, Rest...>>()(isolate, deque);
}



    template<class T, std::size_t N> v8::Local<v8::Value>
CastToJS<std::array<T, N>>::operator()(v8::Isolate * isolate, std::array<T, N> & arr) {
    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    // auto size = arr.size();
    for(unsigned int i = 0; i < N; i++) {
        (void)array->Set(context, i, CastToJS<T&>()(isolate, arr[i]));
    }
    return array;
}


template<class T, std::size_t N> v8::Local<v8::Value>
CastToJS<std::array<T, N> const>::operator()(v8::Isolate * isolate, std::array<T, N> const & array){
    return CastToJS<std::array<T, N>>()(isolate, array);
}





//TODO: forward_list

//TODO: stack

//TODO: set

//TODO: unordered_set



/**
* Does NOT transfer ownership.  Original ownership is maintained.
*/
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::unique_ptr<T, Rest...>>::operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> & unique_ptr) {
//    fprintf(stderr, "RELEASING UNIQUE_PTR MEMORY for unique_ptr type %s\n", demangle<T>().c_str());
    return CastToJS<T*>()(isolate, unique_ptr.release());
}

template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::unique_ptr<T, Rest...>>::operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> && unique_ptr) {
//    fprintf(stderr, "RELEASING UNIQUE_PTR MEMORY for unique_ptr type %s\n", demangle<T>().c_str());
    return CastToJS<T*>()(isolate, unique_ptr.release());
}


template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::unique_ptr<T, Rest...> &>::operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> const & unique_ptr) {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());
    return CastToJS<T*>()(isolate, unique_ptr.get());
}
template<class T, class... Rest> v8::Local<v8::Value>
CastToJS<std::unique_ptr<T, Rest...> const &>::operator()(v8::Isolate * isolate, std::unique_ptr<T, Rest...> const & unique_ptr) {
//    fprintf(stderr, "**NOT** releasing UNIQUE_PTR MEMORY for ptr type %s\n", demangle<T>().c_str());
    return CastToJS<T*>()(isolate, unique_ptr.get());
}





/**
* Storing the resulting javascript object does NOT maintain a reference count on the shared object,
*   so the underlying data can disappear out from under the object if all actual shared_ptr references
*   are lost.
*/
template<class T> v8::Local<v8::Value>
CastToJS<std::shared_ptr<T>>::operator()(v8::Isolate * isolate, std::shared_ptr<T> & shared_ptr) {
    return CastToJS<T*>()(isolate, shared_ptr.get());
}

template<class T> v8::Local<v8::Value>
CastToJS<std::shared_ptr<T> const>::operator()(v8::Isolate * isolate, std::shared_ptr<T> const & shared_ptr) {
    return CastToJS<T*>()(isolate, shared_ptr.get());
}



template<template<class...> class VectorTemplate, class ValueType, class... Rest>
v8::Local<v8::Value> cast_to_js_vector_helper(v8::Isolate * isolate, VectorTemplate<ValueType, Rest...> const & vector) {
    using VectorT = VectorTemplate<ValueType, Rest...>;

    assert(isolate->InContext());
    auto context = isolate->GetCurrentContext();
    auto array = v8::Array::New(isolate);
    auto size = vector.size();
    for (unsigned int i = 0; i < size; i++) {
        (void) array->Set(context, i, CastToJS<ValueType &>()(isolate, const_cast<ValueType &>(vector[i])));
    }
    return array;

}


} // end namespace v8toolkit


#ifdef V8TOOLKIT_ENABLE_EASTL_SUPPORT
#include "casts_eastl.hpp"
#endif

#endif // CASTS_HPP
