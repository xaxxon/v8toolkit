#pragma once

#include <iostream>

#include <assert.h>
/**
* Library of standalone helper functions for using V8.   Can be used independently of V8ClassWrapper
*/

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "casts.h"
#include "casts.hpp"


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


template<class T>
struct CallCallable{};

template<class RETURN_TYPE, typename ... PARAMETERS>
struct CallCallable<std::function<RETURN_TYPE(PARAMETERS...)>> {
	void operator()(std::function<RETURN_TYPE(PARAMETERS...)> callable, const v8::FunctionCallbackInfo<v8::Value> & args, PARAMETERS... parameters) {
		args.GetReturnValue().Set(CastToJS<RETURN_TYPE>()(args.GetIsolate(), callable(parameters...)));
	}
};

template<typename ... PARAMETERS>
struct CallCallable<std::function<void(PARAMETERS...)>> {
	void operator()(std::function<void(PARAMETERS...)> callable, const v8::FunctionCallbackInfo<v8::Value> & args, PARAMETERS... parameters) {
		callable(parameters...);
	}
};



/**
* Class for turning a function parameter list into a parameter pack useful for actually calling the function
*/
template<int depth, typename T, typename U> 
struct ParameterBuilder {};


/**
* Specialization for when there are no parameters left to process, so it is time to actually
* call the function
*/	
template<int depth, typename FUNCTION_TYPE, typename RET>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET()>> {
	// the final class in the call chain stores the actual method to be called

	enum {DEPTH=depth, ARITY=0};
	
	// This call method actually calls the function with the specified object and the
	//   parameter pack that was built up via the chain of calls between templated types
	template<typename ... Ts>
	void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		CallCallable<FUNCTION_TYPE>()(function, info, ts...);
	}
};



/**
* specialization that strips off the first remaining parameter off the function type, stores that and then
*   inherits from another instance that either strips the next one off, or if none remaining, actually calls
*   the function
* The function type is specified twice because the first is actually used by the final specialization to hold the 
*   function type while the second one has its input parameter list stripped off one at a time to determine when
*   the inheritance chain ends
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename HEAD, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(HEAD,TAIL...)>> : public ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
	typedef ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
	enum {DEPTH = depth, ARITY=super::ARITY + 1};

	template<typename ... Ts>
	void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		this->super::operator()(function, info, ts..., CastToNative<HEAD>()(info[depth])); 
	}
};



template<int depth, class T>
struct ParameterBuilder<depth, T, std::function<void(const v8::FunctionCallbackInfo<v8::Value>&)>>
{
	void operator()(std::function<void(const v8::FunctionCallbackInfo<v8::Value> &)> function, const v8::FunctionCallbackInfo<v8::Value> & info) {
		function(info);
	}
};



template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, std::function<R(Args...)> f)
{
	auto copy = new std::function<R(Args...)>(f);
	return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto callable = *(std::function<R(Args...)>*)v8::External::Cast(*(args.Data()))->Value();
		ParameterBuilder<0, std::function<R(Args...)>, std::function<R(Args...)>>()(callable, args);
	}, v8::External::New(isolate, (void*)copy));
}



template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, R(*f)(Args...))
{
	auto callable = new std::function<R(Args...)>(f);
	return make_function_template(isolate, *callable);
}



/**
* Helper to both create a function template and bind it with the specified name to the specified object template
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, v8::Handle<v8::ObjectTemplate> & object_template, const char * name, std::function<R(Args...)> function) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}

template<class R, class... Args>
void add_function(v8::Isolate * isolate, v8::Handle<v8::ObjectTemplate> & object_template, const char * name, R(*function)(Args...)) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}

// add a function that directly handles the v8 callback data
void add_function(v8::Isolate * isolate, v8::Handle<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&)) {
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
	printf("Adding print functions to javascript\n");
	add_function(isolate, global_template, "print",    [](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, false);});
	add_function(isolate, global_template, "println",  [](const v8::FunctionCallbackInfo<v8::Value>& args){print_helper(args, true);});
	add_function(isolate, global_template, "printobj", [](const v8::FunctionCallbackInfo<v8::Value>& args){printobj(args);});
}

template<class T, class U>
struct Bind{};

template<class CLASS_TYPE, class R, class... Args>	
struct Bind<CLASS_TYPE, R(CLASS_TYPE::*)(Args...)> {
	
	Bind(CLASS_TYPE & object, R(CLASS_TYPE::*method)(Args...) ) :
	  object(object), method(method){}
	
	CLASS_TYPE & object;
	R(CLASS_TYPE::*method)(Args...);
	
	R operator()(Args... params){
	    return (object.*method)(params...);	
	}
};

template <class CLASS, class R, class... Args>
std::function<R(Args...)> bind(CLASS & object, R(CLASS::*method)(Args...))
{
	auto bind = new Bind<CLASS, R(CLASS::*)(Args...)>(object, method);
	std::function<R(Args...)> sf(*bind);
	return sf;
}


template<class CALLABLE>
void scoped_run(v8::Isolate * isolate, CALLABLE callable)
{
	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);
	
	if (isolate->InContext()) {
		auto context = isolate->GetCurrentContext();
		v8::Context::Scope context_scope(context);
		callable();
	} else {
		callable();
	}
}

template<class CALLABLE>
void scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, CALLABLE callable)
{
	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);
	v8::Context::Scope context_scope(context);
	
	callable();
}
