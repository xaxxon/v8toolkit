#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

namespace v8toolkit {

template<typename T>
struct CastToNative;

/**
* Casts from a boxed Javascript type to a native type
*/


// integers
template<>
struct CastToNative<long long> {
	long long operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned long long> {
	unsigned long long operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<long> {
	long operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned long> {
	unsigned long operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<int> {
	int operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned int> {
	unsigned int operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<short> {
	short operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned short> {
	unsigned short operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char> {
	char operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<unsigned char> {
	unsigned char operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<bool> {
	bool operator()(v8::Local<v8::Value> value){return value->ToBoolean()->Value();}
};

template<>
struct CastToNative<wchar_t> {
	wchar_t operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char16_t> {
	char16_t operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char32_t> {
	char32_t operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};

#include <assert.h>

template<class U>
struct CastToNative<std::vector<U>> {
    std::vector<U> operator()(v8::Local<v8::Value> value){assert(false);}
};


// floats
template<>
struct CastToNative<float> {
	float operator()(v8::Local<v8::Value> value){return value->ToNumber()->Value();}
};
template<>
struct CastToNative<double> {
	double operator()(v8::Local<v8::Value> value){return value->ToNumber()->Value();}
};
template<>
struct CastToNative<long double> {
	long double operator()(v8::Local<v8::Value> value){return value->ToNumber()->Value();}
};


// strings
template<>
struct CastToNative<char *> {
	char * operator()(v8::Local<v8::Value> value){return *v8::String::Utf8Value(value);}
};
template<>
struct CastToNative<const char *> {
	const char * operator()(v8::Local<v8::Value> value){return CastToNative<char *>()(value);}
};
template<>
struct CastToNative<std::string> {
	std::string operator()(v8::Local<v8::Value> value){return std::string(CastToNative<char *>()(value));}
};

template<>
struct CastToNative<const std::string> {
	const std::string operator()(v8::Local<v8::Value> value){return std::string(CastToNative<char *>()(value));}
};



/**
* Casts from a native type to a boxed Javascript type
*/

template<typename T>
struct CastToJS;


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
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, char value){return v8::Integer::New(isolate, value);}
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
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, int value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<unsigned int> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned int value){return v8::Integer::New(isolate, value);}
};

template<>
struct CastToJS<long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, long value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<unsigned long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, unsigned long value){return v8::Integer::New(isolate, value);}
};

template<>
struct CastToJS<long long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, size_t value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<unsigned long long> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, size_t value){return v8::Integer::New(isolate, value);}
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
// template<>
// struct CastToJS<const std::basic_string<char>> {
//     v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::basic_string<char> & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
// };
template<>
struct CastToJS<std::string> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::string & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
};
template<>
struct CastToJS<const std::string> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, const std::string & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
};




template<>
struct CastToJS<v8::Local<v8::Object>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Local<v8::Object> object){
		return v8::Local<v8::Value>::New(isolate, object);
	}
};
template<>
struct CastToJS<v8::Local<v8::Value>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value){
		return value;
	}
};


/**
* supports vectors containing any type also supported by CastToJS to javascript arrays
*/
template<class U>
struct CastToJS<std::vector<U>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::vector<U> vector){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext();
        auto array = v8::Array::New(isolate);
        auto size = vector.size();
        for(int i = 0; i < size; i++) {
            (void)array->Set(context, i, CastToJS<U>()(isolate, vector.at(i)));
        }
        return array;
    }
};

template<class U>
struct CastToJS<std::list<U>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::list<U> list){
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



template<class A, class B>
struct CastToJS<std::map<A, B>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::map<A, B> map){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext(); 
        auto object = v8::Object::New(isolate);
        for(auto pair : map){
            printf("Adding element from std::map to v8::Object\n");
            (void)object->Set(context, CastToJS<A>()(isolate, pair.first), CastToJS<B>()(isolate, pair.second));
        }
        return object;
    }
};

template<class A, class B>
struct CastToJS<std::unordered_map<A, B>> {
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::unordered_map<A, B> map){
        assert(isolate->InContext());
        auto context = isolate->GetCurrentContext(); 
        auto object = v8::Object::New(isolate);
        for(auto pair : map){
            printf("Adding element from std::map to v8::Object\n");
            (void)object->Set(context, CastToJS<A>()(isolate, pair.first), CastToJS<B>()(isolate, pair.second));
        }
        return object;
    }
};






} // end namespace v8toolkit
