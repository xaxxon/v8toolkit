// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <utility>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include <assert.h>

#include "casts.h"

#include <iostream>
#include <fstream>



// Responsible for calling the actual method and populating the return type for non-void return type methods
template<typename METHOD_TYPE>
struct RunMethod {};

/**
* Specialization for methods that don't return void.  Sets v8::FunctionCallbackInfo::GetReturnValue to value returned by the method
*/
template<typename RETURN_TYPE, typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...)>{
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);

	void operator()(CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters) {
		RETURN_TYPE return_value = (object.*method)(parameters...);
		CastToJS<RETURN_TYPE> cast;
		info.GetReturnValue().Set(cast(info.GetIsolate(), return_value));
	}
};

/**
* Specialization for methods that return void.  No value set for v8::FunctionCallbackInfo::GetReturnValue
*   The javascript call will return the javascript "undefined" value
*/
template<typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<void(CLASS_TYPE::*)(PARAMETERS...)> {
	typedef void(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);
	
	void operator()(CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> &, PARAMETERS... parameters) {
		(object.*method)(parameters...);
	}
};


/**
* Class for turning a function parameter list into a parameter pack useful for actually calling the function
*/
template<int depth, typename T, typename U> 
struct Caller {};


/**
* Specialization for when there are no parameters left to process, so it is time to actually
* call the function
*/	
template<int depth, typename METHOD_TYPE, typename RET, typename CLASS_TYPE>
struct Caller<depth, METHOD_TYPE, RET(CLASS_TYPE::*)()> {
public:
	// the final class in the call chain stores the actual method to be called

	enum {DEPTH=depth, ARITY=0};
	
	// This call method actually calls the method with the specified object and the
	//   parameter pack that was built up via the chain of calls between templated types
	template<typename ... Ts>
	void operator()(METHOD_TYPE method, CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		RunMethod<METHOD_TYPE>()(object, method, info, ts...);
	}
};


/**
* specialization that strips off the first remaining parameter off the method type, stores that and then
*   inherits from another instance that either strips the next one off, or if none remaining, actually calls
*   the method
* The method type is specified twice because the first is actually used by the final specialization to hold the 
*   method type while the second one has its input parameter list stripped off one at a time to determine when
*   the inheritance chain ends
*/
template<int depth, typename METHOD_TYPE, typename CLASS_TYPE, typename RET, typename HEAD, typename...TAIL>
struct Caller<depth, METHOD_TYPE, RET(CLASS_TYPE::*)(HEAD,TAIL...)> : public Caller<depth+1, METHOD_TYPE, RET(CLASS_TYPE::*)(TAIL...)> {
public:
	typedef Caller<depth+1, METHOD_TYPE, RET(CLASS_TYPE::*)(TAIL...)> super;
	enum {DEPTH = depth, ARITY=super::ARITY + 1};


	template<typename ... Ts>
	void operator()(METHOD_TYPE method, CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		CastToNative<HEAD> cast;
		this->super::operator()(method, object, info, ts..., cast(info[depth])); 
	}
};



// Provides a mechanism for creating javascript-ready objects from an arbitrary C++ class
// Can provide a JS constructor method or wrap objects created in another c++ function
template<class T>
class V8ClassWrapper {
private:
	
	static std::map<v8::Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;
	
	// Common tasks to do for any new js object regardless of how it is created
	static void _initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object) {
	    js_object->SetInternalField(0, v8::External::New(isolate, cpp_object));
		
		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));
		
		// set up a callback so we can clean up our internal object when the javascript
		//   object is garbage collected
		auto callback_data = new SetWeakCallbackParameter();
		callback_data->first = cpp_object;
		callback_data->second.Reset(isolate, js_object);
		callback_data->second.SetWeak(callback_data, V8ClassWrapper<T>::v8_destructor);
	}
	
	// users of the library should call get_instance, not the constructor directly
	V8ClassWrapper(v8::Isolate * isolate) : isolate(isolate) {
		// this is a bit weird and there's probably a better way, but create an unbound functiontemplate for use
		//   when wrapping an object that is created outside javascript when there is no way to create that
		//   class type directly from javascript (no js constructor)
		this->constructor_templates.push_back(v8::FunctionTemplate::New(isolate));
	}
	
protected:
	// these are tightly tied, as a FunctionTemplate is only valid in the isolate it was created with
	std::vector<v8::Local<v8::FunctionTemplate>> constructor_templates;
	v8::Isolate * isolate;
	bool member_or_method_added = false;
	
public:
	
	/**
	* Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
	* For each isolate you need to add constructors/methods/members separately.
	*/
	static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate) {
		if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
			isolate_to_wrapper_map.insert(std::make_pair(isolate, new V8ClassWrapper<T>(isolate)));
		}
		return *isolate_to_wrapper_map[isolate];
	}
	
	/**
	* V8ClassWrapper objects shouldn't be deleted during the normal flow of your program unless the associated isolate
	*   is going away forever.   Things will break otherwise as no additional objects will be able to be created
	*   even though V8 will still present the ability to your javascript (I think)
	*/
	virtual ~V8ClassWrapper(){
		isolate_to_wrapper_map.erase(this->isolate);
	}
	
	/**
	* Creates a javascript method of the specified name which, when called with the "new" keyword, will return
	*   a new object of this type
	*/
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	V8ClassWrapper<T> & add_constructor(std::string js_constructor_name, v8::Local<v8::ObjectTemplate> & parent_template) {
				
		// if you add a constructor after adding a member or method, it will be missing on objects created with
		//   this constructor
		assert(member_or_method_added == false);		
				
		// create a function template even if no javascript constructor will be used so 
		//   FunctionTemplate::InstanceTemplate can be populated.   That way if a javascript constructor is added
		//   later the FunctionTemplate will be ready to go
		auto constructor_template = v8::FunctionTemplate::New(isolate, V8ClassWrapper<T>::v8_constructor<CONSTRUCTOR_PARAMETER_TYPES...>);
		
		// When creating a new object, make sure they have room for a c++ object to shadow it
		constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
		
		// store it so it doesn't go away
		this->constructor_templates.push_back(constructor_template);
		
				
				
		// Add the constructor function to the parent object template (often the global template)
		parent_template->Set(v8::String::NewFromUtf8(isolate, js_constructor_name.c_str()), constructor_template);
		
		this->constructor_templates.push_back(constructor_template);
		
		return *this;
	}
	
	// Not sure if this properly sets the prototype of the new object like when the constructor functiontemplate is called as
	//   a constructor from javascript
	/**
	* Used when wanting to return an object from a c++ function call back to javascript
	*/
	v8::Local<v8::Object> wrap_existing_cpp_object(T * existing_cpp_object) {
		auto new_js_object = this->constructor_templates[0]->InstanceTemplate()->NewInstance();
		_initialize_new_js_object(isolate, new_js_object, existing_cpp_object);
		return new_js_object;
	}
	


	typedef std::function<void(const v8::FunctionCallbackInfo<v8::Value>& info)> StdFunctionCallbackType;
	
	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args) {
		StdFunctionCallbackType * callback_lambda = (StdFunctionCallbackType *)v8::External::Cast(*(args.Data()))->Value();		
		(*callback_lambda)(args);
	}
	
	
	template<typename VALUE_T>
	static void GetterHelper(v8::Local<v8::String> property,
			               const v8::PropertyCallbackInfo<v8::Value>& info) {
		v8::Local<v8::Object> self = info.Holder();				   
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());

		auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
		auto & member_ref = (*member_reference_getter)(cpp_object);
		info.GetReturnValue().Set(CastToJS<VALUE_T>()(info.GetIsolate(), member_ref));
	}

	template<typename VALUE_T>
	static void SetterHelper(v8::Local<v8::String> property, v8::Local<v8::Value> value,
	               const v8::PropertyCallbackInfo<void>& info) {
		v8::Local<v8::Object> self = info.Holder();				   
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());

		auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
		auto & member_ref = (*member_reference_getter)(cpp_object);
	  	member_ref = CastToNative<VALUE_T>()(value);
		auto & m2 = (*member_reference_getter)(cpp_object);
	}
	
	/**
	* Adds a getter and setter method for the specified class member
	* add_member(&ClassName::member_name, "javascript_attribute_name");
	*/
	template<typename MEMBER_TYPE>
	V8ClassWrapper<T> & add_member(MEMBER_TYPE T::* member, std::string member_name) {

		// stop additional constructors from being added
		member_or_method_added = true;
		
		typedef MEMBER_TYPE T::*MEMBER_POINTER_TYPE; 

		// this lambda is shared between the getter and the setter so it can only do work needed by both
		static auto get_member_reference = std::function<MEMBER_TYPE&(T*)>([member](T * cpp_object)->MEMBER_TYPE&{
			return cpp_object->*member;
		});
		for(auto constructor_template : this->constructor_templates) {
			constructor_template->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(isolate, 
				member_name.c_str()), 
				GetterHelper<MEMBER_TYPE>, 
				SetterHelper<MEMBER_TYPE>, 
				v8::External::New(isolate, &get_member_reference));
		}
		return *this;
	}
	
	
	/**
	* adds the ability to call the specified class instance method on an object of this type
	* add_method(&ClassName::method_name, "javascript_attribute_name");
	*/
	template<typename METHOD_TYPE>
	V8ClassWrapper<T> & add_method(METHOD_TYPE method, std::string method_name) {
		
		// stop additional constructors from being added
		member_or_method_added = true;
		
		
		// this is leaked if this ever isn't used anymore
		StdFunctionCallbackType * f = new StdFunctionCallbackType([method](const v8::FunctionCallbackInfo<v8::Value>& info) {

			// get the behind-the-scenes c++ object
			v8::Local<v8::Object> self = info.Holder();
			v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
			void* ptr = wrap->Value();
			auto backing_object_pointer = static_cast<T*>(ptr);
			
			Caller<0, METHOD_TYPE, METHOD_TYPE>()(method, *backing_object_pointer, info);
			
		});
		
		auto function_template = v8::FunctionTemplate::New(this->isolate, callback_helper, v8::External::New(this->isolate, f));
		
		for(auto constructor_template : this->constructor_templates) {
			constructor_template->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		}
		return *this;
	}
	
	typedef std::pair<T*, v8::Global<v8::Object>> SetWeakCallbackParameter;
	
	// Helper for creating objects when "new MyClass" is called from java
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
		
		T * new_cpp_object = call_cpp_constructor<CONSTRUCTOR_PARAMETER_TYPES...>(args, std::index_sequence_for<CONSTRUCTOR_PARAMETER_TYPES...>());
		_initialize_new_js_object(isolate, args.This(), new_cpp_object);
		
		// return the object to the javascript caller
		args.GetReturnValue().Set(args.This());
	}

	template <typename ...Fs, size_t...ns> 
	static T * call_cpp_constructor(const v8::FunctionCallbackInfo<v8::Value> & args, std::index_sequence<ns...>){
		return new T(CastToNative<Fs>()(args[ns])...);
	}

	// Helper for cleaning up the underlying wrapped c++ object when the corresponding javascript object is
	// garbage collected
	static void v8_destructor(const v8::WeakCallbackData<v8::Object, SetWeakCallbackParameter> & data) {
		auto isolate = data.GetIsolate();

		SetWeakCallbackParameter * parameter = data.GetParameter();
		
		// delete our internal c++ object and tell V8 the memory has been removed
		delete parameter->first;
		isolate->AdjustAmountOfExternalAllocatedMemory(-sizeof(T));

		// Clear out the Global<Object> handle
		parameter->second.Reset();

		// Delete the heap-allocated std::pair from v8_constructor
		delete parameter;
	}
};

template <class T> std::map<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T>::isolate_to_wrapper_map;



#include "casts.hpp"

// bog standard allocator code from V8 Docs
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

// random sample class for wrapping - not actually a part of the library
class Point {
public:
	Point() : x_(69), y_(69) {printf("created Point\n");}
	Point(int x, int y) : x_(x), y_(y) { printf("created Point with 2 ints\n");}
	Point(const Point & p) { assert(false); /* This is to help make sure none of the helpers are creating copies */ }
	~Point(){printf("****Point destructor called on %p\n", this);}
	int x_, y_;
	int thing(int z, char * zz){printf("In Point::Thing with this %p x: %d y: %d and input value %d %s\n", this, this->x_, this->y_, z, zz); return z*2;}
	int overloaded_method(char * foo){return 0;}
	int overloaded_method(int foo){return 1;}
	const char * stringthing() {return "hello";}
	void void_func() {}
	
	// returns a new point object that should be managed by V8 GC
	Point * make_point(){return new Point();}
};





int main(int argc, char* argv[]) {
	


	// Initialize V8.
	v8::V8::InitializeICU();
	v8::V8::InitializeExternalStartupData(argv[0]);
	v8::Platform* platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(platform);
	v8::V8::Initialize();

	// an Isolate is a V8 instance where multiple applications can run at the same time, but only 
	//   on thread can be running an Isolate at a time.  

	// an context represents the resources needed to run a javascript program
	//   if a program monkey patches core javascript functionality in one context it won't be 
	//   visible to another context
	//   Local<Context> context = Context::New(isolate);
	//   A context has a global object template, but function templates can be added to it

	// A handle is a reference to a javascript object and while active will stop the object from being 
	//   garbage collected
	//   handles exist within a stack-only allocated handle scope. (cannot new() a handle scope) 
	//   UniquePersistent handle is like a unique_ptr
	//   Persistent handle is must be released manually with ::Reset() method

	// EscapableHandleScope lets you return a handle scope created inside a function, otherwise
	//   all handles created in that function will be destroyed before a value is returned
	//   Return with: return handle_scope.Escape(array);

	// Templates allow c++ functions and objects to be made visible in javascript.
	//   templates are created within a context and must be created in each context they are to be used in
	//   Templates have accessors and interceptors
	//      accessors are tied to specific field names
	//      interceptors are called on ALL field name gets/sets (either by name foo.bar or by index as in foo[2])

	// Templates can have prototype templates to simulate prototypical inheritance



	// Create a new Isolate and make it the current one.
	ArrayBufferAllocator allocator;
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &allocator;
	v8::Isolate* isolate = v8::Isolate::New(create_params);
	{
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a stack-allocated handle scope.
		v8::HandleScope handle_scope(isolate);

		// how to expose global variables as javascript variables "x" and "y"
		// global_templ->SetAccessor(String::NewFromUtf8(isolate, "x"), XGetter, XSetter);
		// global_templ->SetAccessor(String::NewFromUtf8(isolate, "y"), YGetter, YSetter);

		// wrap the constructor and add it to the global template
		// Local<FunctionTemplate> ft = FunctionTemplate::New(isolate, create);
		v8::Local<v8::ObjectTemplate> global_templ = v8::ObjectTemplate::New(isolate);

		// // add the function "four()" to javascript
		// global_templ->Set(v8::String::NewFromUtf8(isolate, "four"), FunctionTemplate::New(isolate, four));

		// make the Point constructor function available to JS
		auto & wrapped_point = V8ClassWrapper<Point>::get_instance(isolate);
		wrapped_point.add_constructor("Point", global_templ);
		wrapped_point.add_constructor("SameAsPoint", global_templ); // in case you want to have the same constructor in two places

		wrapped_point.add_constructor<int,int>("Pii", global_templ);
		wrapped_point.add_method(&Point::thing, "thing");

		// overloaded functions can be individually addressed, but they can't be the same name to javascript
		//   at least not without some serious finagling of storing a mapping between a singlne name and
		//   multiple function templates as well as some sort of "closeness" function for determining
		//   which primitive type parameters most closely match the javascript values provided
		wrapped_point.add_method<int (Point::*)(char *)>(&Point::overloaded_method, "overloaded_method1");
		wrapped_point.add_method<int (Point::*)(int)>(&Point::overloaded_method, "overloaded_method2");
		wrapped_point.add_method(&Point::make_point, "make_point");

		wrapped_point.add_method(&Point::stringthing, "stringthing").add_method(&Point::void_func, "void_func");
		wrapped_point.add_member(&Point::x_, "x");
		wrapped_point.add_member(&Point::y_, "y");

		v8::Local<v8::Context> x_context = v8::Context::New(isolate, NULL, global_templ);
		v8::Context::Scope context_scope_x(x_context);


		// Create a string containing the JavaScript source code.
		auto js_code = get_file_contents("code.js");
		v8::Local<v8::String> source =
		    v8::String::NewFromUtf8(isolate, js_code.c_str(),
		                        v8::NewStringType::kNormal).ToLocalChecked();

		// Compile the source code.
		v8::Local<v8::Script> script = v8::Script::Compile(x_context, source).ToLocalChecked();

		v8::Local<v8::String> source_init =
		    v8::String::NewFromUtf8(isolate, "x=0;",
		                        v8::NewStringType::kNormal).ToLocalChecked();

		v8::Local<v8::Script> script_init = v8::Script::Compile(x_context, source_init).ToLocalChecked();
		(void)script_init->Run(x_context);


		// this code shows that scripts run in the same context keep the values from the previous runs
		for (int i = 0; i < 10; i++) {
		    // Run the script to get the result.
		    v8::Local<v8::Value> result = script->Run(x_context).ToLocalChecked();


		    // Convert the result to an UTF8 string and print it.
		    v8::String::Utf8Value utf8(result);
		    printf("%s\n", *utf8);
		}
	}

	// Dispose the isolate and tear down V8.
	isolate->Dispose();
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	delete platform;
	return 0;
}




// decent example:  http://www.codeproject.com/Articles/29109/Using-V-Google-s-Chrome-JavaScript-Virtual-Machin



// code xecycle on freenode:##c++ gave me to call a function from values in a tuple
//  std::get<i>(t)... turns into std::get<0>(t), std::get<1>(t),....,std::get<last_entry_in_i_sequence -1>(t)
//
// namespace tuple_util_impl {
//
// template <size_t... i, class Function, class Tuple>
// auto spread_call(std::index_sequence<i...>, Function f, Tuple&& t)
//   -> decltype(f(std::get<i>(t)...))
// {
//   return f(std::get<i>(t)...);
// }
//
// } // namespace tuple_util_impl
//
// template <class Function, class Tuple>
// auto spread_call(Function&& f, Tuple&& t) -> decltype(
//   tuple_util_impl::spread_call(
//     std::make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>(),
//     f, t))
// {
//   return tuple_util_impl::spread_call(
//     std::make_index_sequence<std::tuple_size<typename std::decay<Tuple>::type>::value>(),
//     std::forward<Function>(f), std::forward<Tuple>(t));
// }


