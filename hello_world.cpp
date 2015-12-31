// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <map>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

#include <assert.h>

#include "casts.h"

using namespace v8;
using namespace std;


// TODO:
// call non-default constructors for objects based on javascript parameters 
//   var my_class = new MyClass(1,2,3); 
//   right now this ignores the 1,2,3 and uses default constructor
//   use best guess based on parameter types?  or maybe not do this at all



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


#include <iostream>
#include <fstream>

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
	Point() : x_(0), y_(0) {printf("created Point\n");}
	Point(int x, int y) : x_(0), y_(0) { printf("created Point\n");}
	Point(const Point & p) { assert(false); /* This is to help make sure none of the helpers are creating copies */ }
	~Point(){printf("****Point destructor called on %p\n", this);}
	int x_, y_;
	int thing(int z, char * zz){printf("In Point::Thing with this %p x: %d y: %d and input value %d %s\n", this, this->x_, this->y_, z, zz); return z*2;}
	int overloaded_method(char * foo){return 0;}
	int overloaded_method(int foo){return 1;}
	const char * stringthing() {return "hello";}
	
	// returns a new point object that should be managed by V8 GC
	Point * make_point(){return new Point();}
};


// Responsible for calling the actual method and populating the return type for non-void return type methods
template<typename METHOD_TYPE>
struct RunMethod {};

/**
* Specialization for methods that don't return void.  Sets v8::FunctionCallbackInfo::GetReturnValue to the method return
*/
template<typename RETURN_TYPE, typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...)>{
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);
	CLASS_TYPE & object;
	METHOD_TYPE method;
	RunMethod(CLASS_TYPE & object, METHOD_TYPE method) : object(object), method(method) {}

	void operator()(const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters) {
		RETURN_TYPE return_value = (object.*method)(parameters...);
		CastToJS<RETURN_TYPE> cast;
		info.GetReturnValue().Set(cast(info.GetIsolate(), return_value));
	}
};

/**
* Specialization for methods that return void.  No value set for v8::FunctionCallbackInfo::GetReturnValue
*/
template<typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<void(CLASS_TYPE::*)(PARAMETERS...)> {
	typedef void(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);
	CLASS_TYPE & object;
	METHOD_TYPE method;
	RunMethod(CLASS_TYPE & object, METHOD_TYPE method) : object(object), method(method) {}
	
	void operator()(const v8::FunctionCallbackInfo<v8::Value> &, PARAMETERS... parameters) {
		(object.*method)(parameters...);
	}
};


/**
* Class for turning a function parameter list into a parameter pack useful for actually calling the function
*/
template<int depth, typename T, typename U> 
class Caller {};

/**
* Specialization for when there are no parameters left to process, so it is time to actually
* call the function
*/	
template<int depth, typename METHOD_TYPE, typename RET, typename CLASS_TYPE>
class Caller<depth, METHOD_TYPE, RET(CLASS_TYPE::*)()> {
public:
	// the final class in the call chain stores the actual method to be called
	METHOD_TYPE method;
	Caller(METHOD_TYPE method):method(method){}

	enum {DEPTH=depth, ARITY=0};
	
	// This call method actually calls the method with the specified object and the
	//   parameter pack that was built up via the chain of calls between templated types
	template<typename ... Ts>
	void call(CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		RunMethod<METHOD_TYPE> run_method(object, this->method);
		run_method(info, ts...);
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
class Caller<depth, METHOD_TYPE, RET(CLASS_TYPE::*)(HEAD,TAIL...)> : public Caller<depth+1, METHOD_TYPE, RET(CLASS_TYPE::*)(TAIL...)> {
public:
	typedef Caller<depth+1, METHOD_TYPE, RET(CLASS_TYPE::*)(TAIL...)> super;
	typedef RET return_type;
	enum {DEPTH = depth, ARITY=super::ARITY + 1};
	Caller(METHOD_TYPE func):super(func) {printf("Depth: %d\n", DEPTH);}

	template<typename ... Ts>
	void call(CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		CastToNative<HEAD> cast;
		printf("Caller call about to recurse\n");
		this->super::call(object, info, ts..., cast(info[depth])); 
	}
};


// Provides a mechanism for creating javascript-ready objects from an arbitrary C++ class
// Can provide a JS constructor method or wrap objects created in another c++ function
template<class T>
class V8ClassWrapper {
private:
	
	static std::map<Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;
	
	// Common tasks to do for any new js object regardless of how it is created
	static void _initialize_new_js_object(Isolate * isolate, Local<Object> js_object, T * cpp_object) {
	    js_object->SetInternalField(0, External::New(isolate, cpp_object));
		
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
	V8ClassWrapper(Isolate * isolate) : isolate(isolate) {
		// create a function template even if no javascript constructor will be used so 
		//   FunctionTemplate::InstanceTemplate can be populated.   That way if a javascript constructor is added
		//   later the FunctionTemplate will be ready to go
		this->constructor_template = FunctionTemplate::New(isolate, V8ClassWrapper<T>::v8_constructor);
		
		// When creating a new object, make sure they have room for a c++ object to shadow it
		this->constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
		
	}
	
protected:
	// these are tightly tied, as a FunctionTemplate is only valid in the isolate it was created with
	Local<FunctionTemplate> constructor_template;
	Isolate * isolate;
	
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
	V8ClassWrapper<T> & add_constructor(std::string js_constructor_name, Local<ObjectTemplate> & parent_template) {
				
		// Add the constructor function to the parent object template (often the global template)
		parent_template->Set(v8::String::NewFromUtf8(isolate, js_constructor_name.c_str()), this->constructor_template);
		
		return *this;
	}
	
	// Not sure if this properly sets the prototype of the new object like when the constructor functiontemplate is called as
	//   a constructor from javascript
	/**
	* Used when wanting to return an object from a c++ function call back to javascript
	*/
	Local<v8::Object> wrap_existing_cpp_object(T * existing_cpp_object) {
		auto new_js_object = constructor_template->InstanceTemplate()->NewInstance();
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
	static void GetterHelper(Local<String> property,
			               const PropertyCallbackInfo<Value>& info) {
		Local<Object> self = info.Holder();				   
		Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());

		auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
		auto & member_ref = (*member_reference_getter)(cpp_object);
		info.GetReturnValue().Set(CastToJS<VALUE_T>()(info.GetIsolate(), member_ref));
	}

	template<typename VALUE_T>
	static void SetterHelper(Local<String> property, Local<Value> value,
	               const PropertyCallbackInfo<void>& info) {
		Local<Object> self = info.Holder();				   
		Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
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
		typedef MEMBER_TYPE T::*MEMBER_POINTER_TYPE; 

		// this lambda is shared between the getter and the setter so it can only do work needed by both
		auto get_member_reference = new std::function<MEMBER_TYPE&(T*)>([member](T * cpp_object)->MEMBER_TYPE&{
			return cpp_object->*member;
		});
		constructor_template->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, "x"), GetterHelper<MEMBER_TYPE>, SetterHelper<MEMBER_TYPE>, v8::External::New(isolate, get_member_reference));
		
		return *this;
	}
	
	
	/**
	* adds the ability to call the specified class instance method on an object of this type
	* add_method(&ClassName::method_name, "javascript_attribute_name");
	*/
	template<typename METHOD_TYPE>
	V8ClassWrapper<T> & add_method(METHOD_TYPE method, std::string method_name) {
		
		// this is leaked if this ever isn't used anymore
		StdFunctionCallbackType * f = new StdFunctionCallbackType([method](const v8::FunctionCallbackInfo<v8::Value>& info) {
			Caller<0, METHOD_TYPE, METHOD_TYPE> caller(method);

			// get the behind-the-scenes c++ object
			Local<Object> self = info.Holder();
			Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
			void* ptr = wrap->Value();
			auto backing_object_pointer = static_cast<T*>(ptr);
			
			caller.call(*backing_object_pointer, info);
			
		});
		
		auto function_template = FunctionTemplate::New(this->isolate, callback_helper, External::New(this->isolate, f));
		this->constructor_template->PrototypeTemplate()->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		
		return *this;
	}
	
	typedef std::pair<T*, Global<Object>> SetWeakCallbackParameter;
	
	// Helper for creating objects when "new MyClass" is called from java
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
		
		// Create the new c++ object and bind it to the javascript object
		T * t = new T();
		printf("Created new object at %p\n", t);

		_initialize_new_js_object(isolate, args.This(), t);
		
		// return the object to the javascript caller
		args.GetReturnValue().Set(args.This());
	}

	// Helper for cleaning up the underlying wrapped c++ object when the corresponding javascript object is
	// garbage collected
	static void v8_destructor(const v8::WeakCallbackData<v8::Object, SetWeakCallbackParameter> & data) {
		auto isolate = data.GetIsolate();
	
		SetWeakCallbackParameter * parameter = data.GetParameter();
		printf("In destructor with callback data at %p\n", parameter);
		
		// delete our internal c++ object and tell V8 the memory has been removed
		delete parameter->first;
		isolate->AdjustAmountOfExternalAllocatedMemory(-sizeof(T));
		
		// Clear out the Global<Object> handle
		parameter->second.Reset();
		
		// Delete the heap-allocated std::pair from v8_constructor
		delete parameter;
	}
};

template <class T> std::map<Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T>::isolate_to_wrapper_map;



#include "casts.hpp"

int main(int argc, char* argv[]) {
	
  // Initialize V8.
  V8::InitializeICU();
  V8::InitializeExternalStartupData(argv[0]);
  Platform* platform = platform::CreateDefaultPlatform();
  V8::InitializePlatform(platform);
  V8::Initialize();

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
  Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = &allocator;
  Isolate* isolate = Isolate::New(create_params);
  {
    Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    HandleScope handle_scope(isolate);

	// how to expose global variables as javascript variables "x" and "y"
    // global_templ->SetAccessor(String::NewFromUtf8(isolate, "x"), XGetter, XSetter);
    // global_templ->SetAccessor(String::NewFromUtf8(isolate, "y"), YGetter, YSetter);
	
	// wrap the constructor and add it to the global template
	// Local<FunctionTemplate> ft = FunctionTemplate::New(isolate, create);
    Local<ObjectTemplate> global_templ = ObjectTemplate::New(isolate);
	
	// // add the function "four()" to javascript
	// global_templ->Set(v8::String::NewFromUtf8(isolate, "four"), FunctionTemplate::New(isolate, four));
	
	// make the Point constructor function available to JS
	auto & wrapped_point = V8ClassWrapper<Point>::get_instance(isolate).add_constructor("Point", global_templ).add_method(&Point::thing, "thing");
	// wrapped_point
	
	// overloaded functions can be individually addressed, but they can't be the same name to javascript
	//   at least not without some serious finagling of storing a mapping between a singlne name and 
	//   multiple function templates as well as some sort of "closeness" function for determining
	//   which primitive type parameters most closely match the javascript values provided
	wrapped_point.add_method<int (Point::*)(char *)>(&Point::overloaded_method, "overloaded_method1");
	wrapped_point.add_method<int (Point::*)(int)>(&Point::overloaded_method, "overloaded_method2");
	wrapped_point.add_method(&Point::make_point, "make_point");
	
	wrapped_point.add_method(&Point::stringthing, "stringthing");
	wrapped_point.add_member(&Point::x_, "x");
	wrapped_point.add_member(&Point::y_, "y");
	
	
	// Local<FunctionTemplate> point_constructor = FunctionTemplate::New(isolate, v8_Point);
	// point_constructor->InstanceTemplate()->SetInternalFieldCount(1);
	//     point_constructor->InstanceTemplate()->SetAccessor(String::NewFromUtf8(isolate, "x"), GetPointX, SetPointX);
	// global_templ->Set(v8::String::NewFromUtf8(isolate, "Point"), point_constructor);
		
	
    Local<Context> x_context = Context::New(isolate, NULL, global_templ);
	Context::Scope context_scope_x(x_context);
	
		
    // Create a string containing the JavaScript source code.
	auto js_code = get_file_contents("code.js");
    Local<String> source =
        String::NewFromUtf8(isolate, js_code.c_str(),
                            NewStringType::kNormal).ToLocalChecked();

    // Compile the source code.
    Local<Script> script = Script::Compile(x_context, source).ToLocalChecked();

    // Run the script to get the result.
    Local<Value> result = script->Run(x_context).ToLocalChecked();

    // Convert the result to an UTF8 string and print it.
    String::Utf8Value utf8(result);
    printf("%s\n", *utf8);
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  V8::Dispose();
  V8::ShutdownPlatform();
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


