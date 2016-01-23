	#pragma once

#include <iostream>

#include <assert.h>
/**
* Library of standalone helper functions for using V8.   Can be used independently of V8ClassWrapper
*/

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include "casts.hpp"


namespace v8toolkit {

/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
*/
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

/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
*/
template<class CALLABLE>
void scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, CALLABLE callable)
{
	v8::Isolate::Scope isolate_scope(isolate);
	v8::HandleScope handle_scope(isolate);
	v8::Context::Scope context_scope(context);

	callable();
}



template<class T>
void for_each_value(v8::Local<v8::Context> context, v8::Local<v8::Value> value, T callable) {
	
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



template<class T>
void for_each_own_property(v8::Local<v8::Context> context, v8::Local<v8::Object> object, T callable)
{
	auto own_properties = object->GetOwnPropertyNames(context).ToLocalChecked();
	for_each_value(context, own_properties, [&object, &context, &callable](v8::Local<v8::Value> property_name){
		auto property_value = object->Get(context, property_name);
		
		callable(property_name, property_value.ToLocalChecked());
	});
}



/**
* parses v8-related flags and removes them, adjusting argc as needed
*/
void process_v8_flags(int & argc, char ** argv)
{
	v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
}


/**
* exposes the garbage collector to javascript
* same as passing --expose-gc as a command-line flag
* To encourage javascript garbage collection run from c++, use: 
*   while(!v8::Isolate::IdleNotificationDeadline([time])) {};
*/	
void expose_gc()
{
	static const char * EXPOSE_GC = "--expose-gc";
	v8::V8::SetFlagsFromString(EXPOSE_GC, strlen(EXPOSE_GC));	
}

/**
* Functor to call a given std::function and, if it has a non-null return value, return its value back to javascript
*/
template<class T>
struct CallCallable{};

/**
* specialization for functions with a non-void return type so the value is sent back to javascript
*/
template<class R, typename ... Args>
struct CallCallable<std::function<R(Args...)>> {
	void operator()(std::function<R(Args...)> callable, const v8::FunctionCallbackInfo<v8::Value> & info, Args... args) {
		info.GetReturnValue().Set(v8toolkit::CastToJS<R>()(info.GetIsolate(), callable(args...)));
	}
};

/**
* specialization for functions with a void return type and there is nothing to be sent back to javascript
*/
template<typename ... Args>
struct CallCallable<std::function<void(Args...)>> {
	void operator()(std::function<void(Args...)> callable, const v8::FunctionCallbackInfo<v8::Value> & info, Args... args) {
		callable(args...);
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



/**
* Specialization to handle functions that want the javascript callback info directly
* Useful for things that want to handle multiple, untyped arguments in a custom way (like the print functions provided in this library)
* Any return value must be handled directly by the function itself by populating the "info" parameter
*/
template<int depth, class T>
struct ParameterBuilder<depth, T, std::function<void(const v8::FunctionCallbackInfo<v8::Value>&)>>
{
	void operator()(std::function<void(const v8::FunctionCallbackInfo<v8::Value> &)> function, const v8::FunctionCallbackInfo<v8::Value> & info) {
		function(info);
	}
};



/**
* Creates a function template from a std::function
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, std::function<R(Args...)> f)
{
	auto copy = new std::function<R(Args...)>(f);
	return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto callable = *(std::function<R(Args...)>*)v8::External::Cast(*(args.Data()))->Value();
		ParameterBuilder<0, std::function<R(Args...)>, std::function<R(Args...)>>()(callable, args);
	}, v8::External::New(isolate, (void*)copy));
}



template<class R, class CLASS, class... Args>
std::function<R(Args...)> make_std_function_from_callable(R(CLASS::*f)(Args...) const, CLASS callable ) 
{
	return std::function<R(Args...)>(callable);
}



template<class T>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, T callable) 
{
	return make_function_template(isolate, make_std_function_from_callable(&T::operator(), callable));
}



/**
* Creates a function template from a c-style function pointer
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, R(*f)(Args...))
{
	return make_function_template(isolate, std::function<R(Args...)>(f));
}


/**
* Helper to both create a function template and bind it with the specified name to the specified object template
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, std::function<R(Args...)> function) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}

template<class T>
void add_function(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, T callable) {
	object_template->Set(isolate, name, make_function_template(isolate, callable));
}

template<class R, class... Args>
void add_function(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, R(*function)(Args...)) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}

template<class T>
void add_function(v8::Local<v8::Context> & context, v8::Local<v8::Object> & object, const char * name, T callable) 
{
	auto isolate = context->GetIsolate();
	scoped_run(isolate, context, [&](){
		auto function_template = make_function_template(isolate, callable);
		auto function = function_template->GetFunction();
		(void)object->Set(context, v8::String::NewFromUtf8(isolate, name), function);
	});
}



void add_variable(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, v8::Local<v8::Value> value) 
{
	object_template->Set(isolate, name, value);
}

void add_variable(v8::Local<v8::Context> context, v8::Local<v8::Object> & object, const char * name, v8::Local<v8::Value> value) 
{
	auto isolate = context->GetIsolate();
	(void)object->Set(context, v8::String::NewFromUtf8(isolate, name), value);
}



/**
* add a function that directly handles the v8 callback data
* explicit function typing needed to coerce non-capturing lambdas into c-style function pointers
*/
void add_function(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&)) {
	object_template->Set(isolate, name, make_function_template(isolate, function));
}


// helper for getting exposed variables
template<class VARIABLE_TYPE>
void _variable_getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
	auto isolate = info.GetIsolate();
	info.GetReturnValue().Set(CastToJS<VARIABLE_TYPE>()(isolate, *(VARIABLE_TYPE*)v8::External::Cast(*(info.Data()))->Value()));
}

// helper for setting exposed variables
template<class VARIABLE_TYPE>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) 
{
	auto isolate = info.GetIsolate();
	*(VARIABLE_TYPE*)v8::External::Cast(*(info.Data()))->Value() = CastToNative<VARIABLE_TYPE>()(value);
}


/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads and writes to the variable
*/
template<class VARIABLE_TYPE>
void expose_variable(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, VARIABLE_TYPE & variable) {
	object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name), _variable_getter<VARIABLE_TYPE>, _variable_setter<VARIABLE_TYPE>, v8::External::New(isolate, &variable));
}
/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads to the variable.  Writes are ignored.
*/
template<class VARIABLE_TYPE>
void expose_variable_readonly(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> & object_template, const char * name, VARIABLE_TYPE & variable) {
	object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name), _variable_getter<VARIABLE_TYPE>, 0, v8::External::New(isolate, &variable));
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


#ifdef USE_BOOST

} // end the v8toolkit namespace temporarily to import boost::format
#include <boost/format.hpp> // only include this if USE_BOOST is defined
namespace v8toolkit { // re-start the namespace
	
	
// Returns the values in a FunctionCallbackInfo object breaking out first-level arrays into their
//   contained values (but not subsequent arrays for no particular reason)
std::vector<v8::Local<v8::Value>> get_all_values(const v8::FunctionCallbackInfo<v8::Value>& args, int depth = 1) {
	std::vector<v8::Local<v8::Value>> values;
	
	auto isolate = args.GetIsolate();
	auto context = isolate->GetCurrentContext();
	
	for (int i = 0; i < args.Length(); i++) {
		if (args[i]->IsArray()) {
			auto array = v8::Object::Cast(*args[i]);
			int i = 0;
			while(array->Has(context, i).FromMaybe(false)) {
				values.push_back(array->Get(context, i).ToLocalChecked());
				i++;
			}
		} else {
			values.push_back(args[i]);
		}
	}
	return values;
}


// takes a format string and some javascript objects and does a printf-style print using boost::format
// fills missing parameters with empty strings and prints any extra parameters with spaces between them
void _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {
	auto values = get_all_values(args);
	
	if (args.Length() > 0) {
		auto string = *v8::String::Utf8Value(values[0]);
		auto format = boost::format(string);

		int i;
		for (i = 1; format.remaining_args() > 0; i++) {
			if (i < values.size()) {
				format % *v8::String::Utf8Value(values[i]);
			} else {
				format % "";
			}
		}
		std::cout << format;
		while (i < values.size()) {
			std::cout << " " << *v8::String::Utf8Value(values[i]);
			i++;
		}
	}
	if (append_newline) {
		std::cout << std::endl;
	}
}

#endif // USE_BOOST



// prints out arguments with a space between them
void _print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {

	auto values = get_all_values(args);
	int i = 0;
	for (auto value : values) {
		if (i > 0) {
			std::cout << " ";
		}
		std::cout << *v8::String::Utf8Value(value);
		i++;	
	}
	if (append_newline) {
		std::cout << std::endl;
	}

}



// prints out information about the guts of an object
void printobj(const v8::FunctionCallbackInfo<v8::Value>& args) {
	auto isolate = args.GetIsolate();
	auto context = isolate->GetCurrentContext();
	for (int i = 0; i < args.Length(); i++) {
		auto object = args[i]->ToObject(context).ToLocalChecked();
		if(object->InternalFieldCount() > 0) {
			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
			printf(">>> Object %p: %s\n", wrap->Value(), *v8::String::Utf8Value(args[i]));
		} else {
			printf(">>> Object does not appear to be a wrapped c++ class (no internal fields): %s\n", *v8::String::Utf8Value(args[i]));
		}
		
		printf("Object has the following own properties\n");
		for_each_own_property(context, object, [](v8::Local<v8::Value> name, v8::Local<v8::Value> value){
			printf(">>> %s: %s\n", *v8::String::Utf8Value(name),*v8::String::Utf8Value(value));
		});
		printf("End of object's own properties\n");
	}
}



// call this to add a function called "print" to whatever object template you pass in (probably the global one)
// if the first parameter is a format string, the appropriate number of following parameters will be used to fill the format
//   any additional strings will be printed, space separated, after the format string.   If the first string has no formatting
//   in it, all strings will simply be printed space-separated.   If there are not enough parameters to fulfill the format, the empty
//   string will be used
// Currently there is no way to print strings that look like formatting strings but aren't.
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> object_template )
{
#ifdef USE_BOOST
	add_function(isolate, object_template, "printf",    [](const v8::FunctionCallbackInfo<v8::Value>& args){_printf_helper(args, false);});
	add_function(isolate, object_template, "printfln",  [](const v8::FunctionCallbackInfo<v8::Value>& args){_printf_helper(args, true);});
#endif
	add_function(isolate, object_template, "print",    [](const v8::FunctionCallbackInfo<v8::Value>& args){_print_helper(args, false);});
	add_function(isolate, object_template, "println",  [](const v8::FunctionCallbackInfo<v8::Value>& args){_print_helper(args, true);});

	add_function(isolate, object_template, "printobj", [](const v8::FunctionCallbackInfo<v8::Value>& args){printobj(args);});
}


/**
* Accepts an object and a method on that object to be called later via its operator()
* Does not require knowledge of how many parameters the method takes or any placeholder arguments
* Can be wrapped with a std::function
*/
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


/**
* Helper function to create a Bind object using type deduction and wrap it in a
* std::function object.
*/
template <class CLASS, class R, class... Args>
std::function<R(Args...)> bind(CLASS & object, R(CLASS::*method)(Args...))
{
	return std::function<R(Args...)>(Bind<CLASS, R(CLASS::*)(Args...)>(object, method));
}



/**
* Example allocator code from V8 Docs
*/
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};



// helper for testing code, not a part of the library
// read the contents of the file and return it as a std::string
std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}



/**
* adds 'require' method to javascript to emulate node require.
* Adds an self-referential "global" alias to the global object
* Must be run after the context is created so "global" can refer to the global object
*   (if this could be changed, it should be, but I don't know how to do it beforehand)
*/
// Node modules must share the global object with the running script that requires them
// node modules expect "global" to be an alias to the global object
void add_require(v8::Isolate * isolate, v8::Local<v8::Context> context, std::vector<std::string> & paths)
{
	static bool require_added = false;
	if(require_added) {
		printf("Require already added, not doing anything\n");
	}
	require_added = true;
	auto global_object = context->Global();
	
	
	(void)add_function(context, global_object, "require", [isolate, paths](std::string filename)->v8::Local<v8::Value>{
		if(filename.find_first_of("..") == std::string::npos) {
			printf("require() attempted to use a path with more than one . in a row (disallowed as simple algorithm to stop tricky paths)");
			return v8::Object::New(isolate);
		}
		
		auto context = isolate->GetCurrentContext();
		for(auto path : paths) {
			try {
											
				// create a new context for it (this may be the wrong thing to do)
				auto module_global_template = v8::ObjectTemplate::New(isolate);		
				add_print(isolate, module_global_template);			

				// Create module context
				auto module_context = v8::Context::New(isolate, nullptr, module_global_template);
				
				// Get module global object
				auto module_global_object = module_context->Global();
				
				// set up the module and exports stuff
				auto module_object = v8::Object::New(isolate);
				add_variable(module_context, module_object, "exports", v8::Object::New(isolate));
				add_variable(module_context, module_global_object, "module", module_object);
				(void)module_global_object->Set(module_context, v8::String::NewFromUtf8(isolate, "global"), module_global_object);
				


				auto contents = get_file_contents((path + filename).c_str());
				v8::Local<v8::String> source =
				    v8::String::NewFromUtf8(isolate, contents.c_str(),
				                        	v8::NewStringType::kNormal).ToLocalChecked();


				// Compile the source code.
				v8::Local<v8::Script> script = v8::Script::Compile(module_context, source).ToLocalChecked();

				printf("About to start running script\n");
				auto result = script->Run(context);
				
				return module_object->Get(module_context, v8::String::NewFromUtf8(isolate, "exports")).ToLocalChecked();

			}catch(...) {

			}
			// if any failures, try the next path if it exists
		}
		return v8::Object::New(isolate);

	});
}











} // end v8toolkit namespace


