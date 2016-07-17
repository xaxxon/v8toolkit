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

namespace v8toolkit {


#define V8_TOOLKIT_COMMA ,

#define CAST_TO_NATIVE_WITH_CONST(TYPE, TEMPLATE) \
template<TEMPLATE> \
struct CastToNative<TYPE>{ \
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
        return CastToNative<const TYPE>()(isolate, value); \
    } \
}; \
\
template<TEMPLATE> \
struct CastToNative<const TYPE> { \
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const

#define CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(TYPE) \
template<> \
struct CastToNative<TYPE> { \
    TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const; \
}; \
\
template<> \
struct CastToNative<const TYPE>{ \
    const TYPE operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const { \
        return CastToNative<TYPE>()(isolate, value); \
    } \
}; \
inline TYPE CastToNative<TYPE>::operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const




#define CAST_TO_JS_PRIMITIVE_WITH_CONST(TYPE) \
template<> struct CastToJS<const TYPE> { \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const TYPE value) const; \
}; \
template<> \
struct CastToJS<TYPE> { \
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, TYPE value) const {return CastToJS<const TYPE>()(isolate, value);} \
}; \
inline v8::Local<v8::Value>  CastToJS<const TYPE>::operator()(v8::Isolate * isolate, const TYPE value) const


/**
* Casts from a boxed Javascript type to a native type
*/
template<typename T, class = void>
struct CastToNative;


template<>
struct CastToNative<void> {
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {}
};

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(bool){return static_cast<bool>(value->ToBoolean()->Value());}



// integers
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long long){return static_cast<long long>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned long long){return static_cast<unsigned long long>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long){return static_cast<long>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned long){return static_cast<unsigned long>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(int) {return static_cast<int>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned int){return static_cast<unsigned int>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(short){return static_cast<short>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned short){return static_cast<unsigned short>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char){return static_cast<char>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(unsigned char){return static_cast<unsigned char>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(wchar_t){return static_cast<wchar_t>(value->ToInteger()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char16_t){return static_cast<char16_t>(value->ToInteger()->Value());}

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(char32_t){return static_cast<char32_t>(value->ToInteger()->Value());}


template<class Return, class... Params>
struct CastToNative<std::function<Return(Params...)>> {
    std::function<Return(Params...)> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const;
};



CAST_TO_NATIVE_WITH_CONST(std::pair<FirstT V8_TOOLKIT_COMMA SecondT>, class FirstT V8_TOOLKIT_COMMA class SecondT) {
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
        const auto &native_first = v8toolkit::CastToNative<FirstT>()(isolate, first);
        const auto &native_second = v8toolkit::CastToNative<SecondT>()(isolate, second);
        return std::pair<FirstT, SecondT>(native_first, native_second);

    } else {
        printf("cast to native std::pair got %s\n", stringify_value(isolate, value).c_str());
        auto error = "CastToNative<std::pair<>> requires an array, but another type was provided";
        isolate->ThrowException(v8::String::NewFromUtf8(isolate, error));
        throw v8toolkit::CastException(error);
    }
}
};



CAST_TO_NATIVE_WITH_CONST(std::vector<ElementType V8_TOOLKIT_COMMA Rest...>, class ElementType V8_TOOLKIT_COMMA class... Rest)
{
    auto context = isolate->GetCurrentContext();
    std::vector<ElementType, Rest...> v;
    if(value->IsArray()) {
        auto array = v8::Local<v8::Object>::Cast(value);
        auto array_length = get_array_length(isolate, array);
        for(int i = 0; i < array_length; i++) {
            auto value = array->Get(context, i).ToLocalChecked();
            v.push_back(CastToNative<ElementType>()(isolate, value));
        }
    } else {
        isolate->ThrowException(v8::String::NewFromUtf8(isolate,"Function requires an array, but another type was provided"));
    }
    return v;
}};


CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(float){return static_cast<float>(value->ToNumber()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(double){return static_cast<double>(value->ToNumber()->Value());}
CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(long double){return static_cast<long double>(value->ToNumber()->Value());}



template<>
struct CastToNative<v8::Local<v8::Function>> {
	v8::Local<v8::Function> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        if(value->IsFunction()) {
            return v8::Local<v8::Function>::Cast(value);
        } else {
            isolate->ThrowException(v8::String::NewFromUtf8(isolate,"Function requires a v8::Function, but another type was provided"));
            return v8::Local<v8::Function>();
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
    return std::unique_ptr<char[]>(strdup(*v8::String::Utf8Value(value)));
  }
};
template<>
struct CastToNative<const char *> {
  std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return CastToNative<char *>()(isolate, value); }
};

CAST_TO_NATIVE_PRIMITIVE_WITH_CONST(std::string){return std::string(*v8::String::Utf8Value(value));}



//
//template<class T>
//struct CastToNative<T&&> {
//    T && operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
//
//        // if T is a user-defined type
//        if (value->IsObject()) {
//            auto object = value->ToObject();
//            if (object->InternalFieldCount() == 1) {
//                auto && result = CastToNative<T>()(isolate, value);
//                // anything else to do that is specific to dealing with moved internal field objects?
//            }
//        }
//
//        // else maybe something wants a vector<string>&& or unique_ptr<int>&&
//        return CastToNative<T>()(isolate, value);
//    }
//};
//
template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<std::is_copy_constructible<T>::value>> {
    std::unique_ptr<T> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        // if T is a user-defined type
        if (value->IsObject()) {
            auto object = value->ToObject();
            if (object->InternalFieldCount() == 1) {
                auto && result = CastToNative<T>()(isolate, value);
                std::unique_ptr<T, Rest...>(CastToNative<T*>()(isolate, value));
            }
        }
        return std::unique_ptr<T, Rest...>(new T(CastToNative<T>()(isolate, value)));
    }
};

template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<!std::is_copy_constructible<T>::value>> {
    std::unique_ptr<T> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        // if T is a user-defined type
        if (value->IsObject()) {
            auto object = value->ToObject();
            if (object->InternalFieldCount() == 1) {
                auto && result = CastToNative<T>()(isolate, value);
                std::unique_ptr<T, Rest...>(CastToNative<T*>()(isolate, value));
            }
        }
        throw CastException("Cannot make unique ptr for type that is not wrapped and not copy constructible");
    }
};


//


    /**
* Casts from a native type to a boxed Javascript type
*/

template<typename T>
struct CastToJS;

CAST_TO_JS_PRIMITIVE_WITH_CONST(bool){return v8::Boolean::New(isolate, value);}

//TODO: Should all these operator()'s be const?
// integers
CAST_TO_JS_PRIMITIVE_WITH_CONST(char){return v8::Integer::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(unsigned char){return v8::Integer::New(isolate, value);}

CAST_TO_JS_PRIMITIVE_WITH_CONST(wchar_t){return v8::Number::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(char16_t){return v8::Integer::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(char32_t){return v8::Integer::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(short){return v8::Integer::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(unsigned short){return v8::Integer::New(isolate, value);}



CAST_TO_JS_PRIMITIVE_WITH_CONST(int){return v8::Number::New(isolate, value);}

CAST_TO_JS_PRIMITIVE_WITH_CONST(unsigned int){return v8::Number::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(long){return v8::Number::New(isolate, value);}

CAST_TO_JS_PRIMITIVE_WITH_CONST(unsigned long){return v8::Number::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(long long){return v8::Number::New(isolate, static_cast<double>(value));}
CAST_TO_JS_PRIMITIVE_WITH_CONST(unsigned long long){return v8::Number::New(isolate, static_cast<double>(value));}



// floats
CAST_TO_JS_PRIMITIVE_WITH_CONST(float){return v8::Number::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(double){return v8::Number::New(isolate, value);}
CAST_TO_JS_PRIMITIVE_WITH_CONST(long double){return v8::Number::New(isolate, value);}


CAST_TO_JS_PRIMITIVE_WITH_CONST(std::string){return v8::String::NewFromUtf8(isolate, value.c_str());}

CAST_TO_JS_PRIMITIVE_WITH_CONST(char *){return v8::String::NewFromUtf8(isolate, value);}

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
    

/**
* supports vectors containing any type also supported by CastToJS to javascript arrays
*/
template<class U, class... Rest>
struct CastToJS<std::vector<U, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::vector<U, Rest...> vector){
        // return CastToJS<std::vector<U, Rest...>*>()(isolate, &vector);
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        auto size = vector.size();
        for(unsigned int i = 0; i < size; i++) {
            (void)array->Set(context, i, CastToJS<U>()(isolate, vector.at(i)));
        }
        return array;
    }
};



/**
* supports lists containing any type also supported by CastToJS to javascript arrays
*/
template<class U, class... Rest>
struct CastToJS<std::list<U, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::list<U, Rest...> list){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        int i = 0;
        for (auto element : list) {
            (void)array->Set(context, i, CastToJS<U>()(isolate, element));
            i++;
        }
        return array;
    }
};


/**
* supports maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest>
struct CastToJS<std::map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::map<A, B, Rest...> & map){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext(); 
        auto object = v8::Object::New(isolate);
        for(auto pair : map){
            (void)object->Set(context, CastToJS<A>()(isolate, pair.first), CastToJS<B>()(isolate, pair.second));
        }
        return object;
    }
};

/**
* supports maps containing any type also supported by CastToJS to javascript arrays
* It creates an object of key => [values...]
* All values are arrays, even if there is only one value in the array.
*/
template<class A, class B, class... Rest>
struct CastToJS<std::multimap<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::multimap<A, B, Rest...> map){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext(); 
        auto object = v8::Object::New(isolate);
        for(auto pair : map){
            auto key = CastToJS<A>()(isolate, pair.first);
            // v8::Local<v8::String> key = v8::String::NewFromUtf8(isolate, "TEST");
            auto value = CastToJS<B>()(isolate, pair.second);
            
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
};


template<class T, class U>
struct CastToJS<std::pair<T, U>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::pair<T, U> & pair){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        (void)array->Set(context, 0, CastToJS<T>()(isolate, pair.first));
        (void)array->Set(context, 1, CastToJS<U>()(isolate, pair.second));
        return array;
    }
//    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::pair<T, U> & pair){
//        assert(isolate->InContext());
//        auto context = isolate->GetCurrentContext();
//        auto array = v8::Array::New(isolate);
//        (void)array->Set(context, 0, CastToJS<T>()(isolate, pair.first));
//        (void)array->Set(context, 1, CastToJS<U>()(isolate, pair.second));
//        return array;
//    }

};

template<int position, class T>
struct CastTupleToJS;

template<class... Args>
struct CastTupleToJS<0, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - 0 - 1;
        
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        (void)array->Set(context, array_position, CastToJS<typename std::tuple_element<array_position, std::tuple<Args...>>::type>()(isolate, std::get<array_position>(tuple)));
        return array;
    }
};

template<int position, class... Args>
struct CastTupleToJS<position, std::tuple<Args...>> {
    v8::Local<v8::Array> operator()(v8::Isolate * isolate, std::tuple<Args...> & tuple){
        constexpr int array_position = sizeof...(Args) - position - 1;
        
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = CastTupleToJS<position - 1, std::tuple<Args...>>()(isolate, tuple);
        (void)array->Set(context, array_position, CastToJS<typename std::tuple_element<array_position, std::tuple<Args...>>::type>()(isolate, std::get<array_position>(tuple)));
        return array;
    }
};



template<class... Args>
struct CastToJS<std::tuple<Args...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::tuple<Args...> tuple) {
        return CastTupleToJS<sizeof...(Args) - 1, std::tuple<Args...>>()(isolate, tuple);
    }
};


/**
* supports unordered_maps containing any type also supported by CastToJS to javascript arrays
*/
template<class A, class B, class... Rest>
struct CastToJS<std::unordered_map<A, B, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unordered_map<A, B, Rest...> map){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext(); 
        auto object = v8::Object::New(isolate);
        for(auto pair : map){
            (void)object->Set(context, CastToJS<A>()(isolate, pair.first), CastToJS<B>()(isolate, pair.second));
        }
        return object;
    }
};


/**
* supports deques containing any type also supported by CastToJS to javascript arrays
*/
template<class T, class... Rest>
struct CastToJS<std::deque<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::deque<T, Rest...> deque){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        auto size = deque.size();
        for(unsigned int i = 0; i < size; i++) {
            (void)array->Set(context, i, CastToJS<T>()(isolate, deque.at(i)));
        }
        return array;
    }    
};


template<class T, std::size_t N>
struct CastToJS<std::array<T, N>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::array<T, N> arr){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        // auto size = arr.size();
        for(unsigned int i = 0; i < N; i++) {
            (void)array->Set(context, i, CastToJS<T>()(isolate, arr.at(i)));
        }
        return array;
    }    
};




//TODO: forward_list

//TODO: stack

//TODO: set

//TODO: unordered_set



/**
* Does NOT transfer ownership.  Original ownership is maintained.
*/
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::unique_ptr<T, Rest...> & unique_ptr) {
        return CastToJS<T*>()(isolate, unique_ptr.get());
    }
};





/**
* Storing the resulting javascript object does NOT maintain a reference count on the shared object,
*   so the underlying data can disappear out from under the object if all actual shared_ptr references
*   are lost.
*/
template<class T>
struct CastToJS<std::shared_ptr<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::shared_ptr<T> & shared_ptr) {
        return CastToJS<T*>()(isolate, shared_ptr.get());
    }
};


} // end namespace v8toolkit



#endif // CASTS_HPP
