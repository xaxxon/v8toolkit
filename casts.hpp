#pragma once

#include <string>

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
	char16_t operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
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
template<>
struct CastToJS<std::string> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::string & value){return v8::String::NewFromUtf8(isolate, value.c_str());}
};


}