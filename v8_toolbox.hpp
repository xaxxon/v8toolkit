#pragma once


#include "include/libplatform/libplatform.h"
#include "include/v8.h"



template<class CALLBACK_FUNCTION>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, CALLBACK_FUNCTION function)
{		
	return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& args){
		(*(CALLBACK_FUNCTION *)v8::External::Cast(*(args.Data()))->Value())(args);
	}, v8::External::New(isolate, (void*)&function));
}

template<class CALLBACK_FUNCTION>
void add_function(v8::Isolate * isolate, v8::Handle<v8::ObjectTemplate> & object_template, const char * name, CALLBACK_FUNCTION function) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}




#include <boost/format.hpp>
static void print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
	if (args.Length() > 0) {
		auto string = *v8::String::Utf8Value(args[0]);
		auto format = boost::format(string);
		
		for (int i = 1; i < args.Length(); i++) {
			format % *v8::String::Utf8Value(args[i]);
		}
		std::cout << ">>> ";
		std::cout << format;	
		if (append_newline) {
			std::cout << std::endl;
		}
	}
}



void printobj(const v8::FunctionCallbackInfo<v8::Value>& args) {
	for (int i = 0; i < args.Length(); i++) {
		auto object = v8::Object::Cast(*args[i]);
		if(object->InternalFieldCount() > 0) {
			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
			printf(">>> Object %p: %s\n", wrap->Value(), *v8::String::Utf8Value(args[i]));
		} else {
			printf(">>> Object does not appear to be a wrapped c++ class (no internal fields): %s\n", *v8::String::Utf8Value(args[i]));
		}
	}
	// v8::Local<v8::Object> self = info.Holder();
	// v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	// T * cpp_object = static_cast<T *>(wrap->Value());
}

// call this to add a function called "print" to whatever object template you pass in (probably the global one)
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> global_template )
{
	v8::HandleScope hs(isolate);
	// auto print_template = v8::FunctionTemplate::New(isolate, &print_callback);
	add_function(isolate, global_template, "print", [&](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, false);});
	add_function(isolate, global_template, "println", [&](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, true);});
	add_function(isolate, global_template, "printobj", [&](const v8::FunctionCallbackInfo<v8::Value>& args){printobj(args);});
	
}


