#pragma once


/**
* Library of standalone helper functions for using V8.   Can be used independently of V8ClassWrapper
*/

#include "include/libplatform/libplatform.h"
#include "include/v8.h"


// parses v8-related flags and removes them, adjusting argc as needed
void process_v8_flags(int & argc, char ** argv)
{
	v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}


// exposes the garbage collector to javascript
// same as passing --expose-gc as a command-line flag
// To "help" javascript garbage collection from c++, use: 
//   while(!V8::IdleNotification()) {};
void expose_gc()
{
	const char * EXPOSE_GC = "--expose-gc";
	v8::V8::SetFlagsFromString("--expose-gc", strlen("--expose-gc"));	
}


template<class CALLBACK_FUNCTION>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, CALLBACK_FUNCTION function)
{		
	return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
		(*(CALLBACK_FUNCTION *)v8::External::Cast(*(args.Data()))->Value())(args);
	}, v8::External::New(isolate, (void*)&function));
}


/**
* Helper to both create a function template and bind it with the specified name to the specified object template
*/
template<class CALLBACK_FUNCTION>
void add_function(v8::Isolate * isolate, v8::Handle<v8::ObjectTemplate> & object_template, const char * name, CALLBACK_FUNCTION function) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}

/**
* Takes a local and creates a weak global reference callback for it
*/
template<class CALLBACK_FUNCTION>
void global_set_weak(v8::Isolate * isolate, v8::Local<v8::Object> & javascript_object, CALLBACK_FUNCTION function)
{
	struct SetWeakCallbackData{
		SetWeakCallbackData(CALLBACK_FUNCTION function, v8::Isolate * isolate, v8::Local<v8::Object> & javascript_object) : 
			function(function) {
				this->global.Reset(isolate, javascript_object);
		}
		CALLBACK_FUNCTION function;
		v8::Global<v8::Object> global;
	};
	
	auto callback_data = new SetWeakCallbackData(function, isolate, javascript_object);
	callback_data->global.template SetWeak<SetWeakCallbackData>(callback_data,
		[](const v8::WeakCallbackData<v8::Object, SetWeakCallbackData> & data){
			SetWeakCallbackData * callback_data = data.GetParameter();			
			callback_data->function();
			callback_data->global.Reset();
			delete callback_data;
		});
}



#include <boost/format.hpp>
// takes a format string and some javascript objects and does a printf-style print using boost::format
static void print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
	if (args.Length() > 0) {
		auto string = *v8::String::Utf8Value(args[0]);
		auto format = boost::format(string);

		int i;
		for (i = 1; format.remaining_args() > 0; i++) {
			if (i < args.Length()) {
				format % *v8::String::Utf8Value(args[i]);
			} else {
				format % "";
			}
		}
		std::cout << ">>> ";
		std::cout << format;
		while (i < args.Length()) {
			std::cout << " " << *v8::String::Utf8Value(args[i]);
			i++;
		}
	}
	if (append_newline) {
		std::cout << std::endl;
	}
}


// prints out information about the guts of an object
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
}

// call this to add a function called "print" to whatever object template you pass in (probably the global one)
// if the first parameter is a format string, the appropriate number of following parameters will be used to fill the format
//   any additional strings will be printed, space separated, after the format string.   If the first string has no formatting
//   in it, all strings will simply be printed space-separated.   If there are not enough parameters to fulfill the format, the empty
//   string will be used
// Currently there is no way to print strings that look like formatting strings but aren't.
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> global_template )
{
	add_function(isolate, global_template, "print", [&](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, false);});
	add_function(isolate, global_template, "println", [&](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, true);});
	add_function(isolate, global_template, "printobj", [&](const v8::FunctionCallbackInfo<v8::Value>& args){printobj(args);});	
}


