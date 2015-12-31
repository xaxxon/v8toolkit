#pragma once


template<>
struct CastToNative<int> {
	int operator()(v8::Local<v8::Value> value){return value->ToInteger()->Value();}
};
template<>
struct CastToNative<char *> {
	char * operator()(v8::Local<v8::Value> value){return *v8::String::Utf8Value(value);}
};
template<>
struct CastToNative<const char *> {
	const char * operator()(v8::Local<v8::Value> value){return CastToNative<char *>()(value);}
};
template<>
struct CastToNative<double> {
	double operator()(v8::Local<v8::Value> value){return value->ToNumber()->Value();}
};
template<>
struct CastToNative<std::string> {
	std::string operator()(v8::Local<v8::Value> value){return std::string(CastToNative<char *>()(value));}
};




template<>
struct CastToJS<int> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, int value){return v8::Integer::New(isolate, value);}
};
template<>
struct CastToJS<double> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, double value){return v8::Number::New(isolate, value);}
};
template<>
struct CastToJS<const char *> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, const char * value){return v8::String::NewFromUtf8(isolate, value);}
};

template<typename T>
struct CastToJS<T*> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){return V8ClassWrapper<T>::wrap_existing_cpp_object(cpp_object);}
};
