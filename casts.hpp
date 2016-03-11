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

#include "v8.h"

namespace v8toolkit {

template<typename T>
struct CastToNative;


/**
* Casts from a boxed Javascript type to a native type
*/
template<>
struct CastToNative<void> {
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {}
};


// integers
template<>
struct CastToNative<long long> {
	long long operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned long long> {
	unsigned long long operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<long> {
	long operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned long> {
	unsigned long operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<int> {
	int operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned int> {
	unsigned int operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<short> {
	short operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned short> {
	unsigned short operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char> {
	char operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned char> {
	unsigned char operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<bool> {
	bool operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToBoolean()->Value();}
};

template<>
struct CastToNative<wchar_t> {
	wchar_t operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char16_t> {
	char16_t operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char32_t> {
	char32_t operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToInteger()->Value();}
};


// TODO: Make sure this is tested
template<class ElementType, class... Rest>
struct CastToNative<std::vector<ElementType, Rest...>> {
    std::vector<ElementType, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        auto context = isolate->GetCurrentContext();
        std::vector<ElementType, Rest...> v;
        if(value->IsArray()) {
            auto array = v8::Local<v8::Object>::Cast(value);
            auto array_length = array->Get(context, v8::String::NewFromUtf8(isolate, "length")).ToLocalChecked()->Uint32Value();
            for(int i = 0; i < array_length; i++) {
                auto value = array->Get(context, i).ToLocalChecked();
                v.push_back(CastToNative<ElementType>()(isolate, value));
            }
        } else {
            isolate->ThrowException(v8::String::NewFromUtf8(isolate,"Function requires a v8::Function, but another type was provided"));
        }
        return v;
    }
};


// floats
template<>
struct CastToNative<float> {
	float operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToNumber()->Value();}
};
template<>
struct CastToNative<double> {
	double operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToNumber()->Value();}
};
template<>
struct CastToNative<long double> {
	long double operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return value->ToNumber()->Value();}
};


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
    char * string = *v8::String::Utf8Value(value);
    auto string_length = strlen(string);
    auto new_string = new char[string_length + 1];
    strncpy(new_string, string, string_length + 1);
    return std::unique_ptr<char[]>(new_string);
  }
};




/**
 * char * and const char * are the only types that don't actually return their own type.  Since a buffer is needed
 *   to store the string, a std::unique_ptr<char[]> is returned.
 */
template<>
struct CastToNative<const char *> {
  std::unique_ptr<char[]>  operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return CastToNative<char *>()(isolate, value); }
};

template<>
struct CastToNative<std::string> {
  std::string operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return std::string(*v8::String::Utf8Value(value));}
};

template<>
struct CastToNative<const std::string> {
  const std::string operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {return std::string(CastToNative<std::string>()(isolate, value));}
};



/**
* Casts from a native type to a boxed Javascript type
*/

template<typename T>
struct CastToJS;

//TODO: Should all these operator()'s be const?
// integers
template<>
struct CastToJS<char> { 
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<unsigned char> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned char value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<wchar_t> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<char16_t> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char16_t value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<char32_t> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char32_t value){return v8::Integer::New(isolate, value);}
};


template<>
struct CastToJS<short> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, short value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<unsigned short> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned short value){return v8::Integer::New(isolate, value);}
};

template<>
struct CastToJS<int> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, int value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<unsigned int> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned int value){return v8::Number::New(isolate, value);}
};

template<>
struct CastToJS<long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, long value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<unsigned long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned long value){return v8::Number::New(isolate, value);}
};

template<>
struct CastToJS<long long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, size_t value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<unsigned long long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, size_t value){return v8::Number::New(isolate, value);}
};


// floats
template<>
struct CastToJS<float> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, float value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<double> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, double value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<long double> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, long double value){return v8::Number::New(isolate, value);}
};

template<>
struct CastToJS<bool> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, bool value){return v8::Boolean::New(isolate, value);}
};


// strings
template<>
struct CastToJS<char *> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char * value){return v8::String::NewFromUtf8(isolate, value);}
};
template<>
struct CastToJS<const char *> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, const char * value){return v8::String::NewFromUtf8(isolate, value);}
};

template<>
struct CastToJS<std::string> {
  v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::string & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
};
template<>
struct CastToJS<const std::string> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::string & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
};


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
		return v8::Local<v8::Value>::New(isolate, object);
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
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Global<v8::Value> & value){
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
template<class T>
struct CastToJS<std::unique_ptr<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::unique_ptr<T> & unique_ptr) {
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
