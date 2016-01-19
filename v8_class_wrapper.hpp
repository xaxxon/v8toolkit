// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <assert.h>

// V8 includes
#include "include/libplatform/libplatform.h"
#include "include/v8.h"

/***
* set of classes for determining what to do do the underlying c++
*   object when the javascript object is garbage collected
*/
template<class T>
struct DestructorBehavior {
	virtual void operator()(T* object) const = 0;

};

template<class T>
struct DestructorBehaviorDelete : DestructorBehavior<T> {
	void operator()(T* object) const {
		printf("Deleting object at %p during V8 garbage collection\n", object);
		delete object;
	}
};

template<class T>
struct DestructorBehaviorLeaveAlone : DestructorBehavior<T> {
	void operator()(T* object) const {
		printf("Not deleting object %p during V8 garbage collection\n", object);
	}
};


// Specialized types that know how to convert from a v8::Value to a primitive type
template<typename T>
struct CastToNative {};


// Responsible for calling the actual method and populating the return type for non-void return type methods
template<typename METHOD_TYPE>
struct RunMethod {};


template<typename CLASS_TYPE, typename METHOD_TYPE, typename ... PARAMETERS>
void run_non_void(CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters);

/**
* Specialization for methods that don't return void.  Sets v8::FunctionCallbackInfo::GetReturnValue to value returned by the method
*/
template<typename RETURN_TYPE, typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...)>{
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);

	void operator()(CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters) {
		run_non_void(object, method, info, parameters...);
	}
};

// const method version 
template<typename RETURN_TYPE, typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...) const>{
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...) const;

	void operator()(const CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters) {
		run_non_void(object, method, info, parameters...);
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

// const method version 
template<typename CLASS_TYPE, typename ... PARAMETERS>
struct RunMethod<void(CLASS_TYPE::*)(PARAMETERS...) const> {
	typedef void(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...) const;

	void operator()(const CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> &, PARAMETERS... parameters) {
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



// simple class to forward calls on to Caller without caring about the const-ness of the method
template<class T>
struct CallerQualifierRemover {};

template<class RETURN_TYPE, class CLASS_TYPE, class ... PARAMETERS>
struct CallerQualifierRemover<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...)> {
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...);
	void operator()(METHOD_TYPE method, CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info) {
		Caller<0, METHOD_TYPE, METHOD_TYPE>()(method, object, info);
	}
};

// take a const method and strip the const off
template<class RETURN_TYPE, class CLASS_TYPE, class ... PARAMETERS>
struct CallerQualifierRemover<RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...) const> {
	typedef RETURN_TYPE(CLASS_TYPE::*METHOD_TYPE)(PARAMETERS...) const;
	void operator()(METHOD_TYPE method, CLASS_TYPE & object, const v8::FunctionCallbackInfo<v8::Value> & info) {
		Caller<0, METHOD_TYPE, RETURN_TYPE(CLASS_TYPE::*)(PARAMETERS...)>()(method, object, info);
	}
};



// Provides a mechanism for creating javascript-ready objects from an arbitrary C++ class
// Can provide a JS constructor method or wrap objects created in another c++ function
template<class T>
class V8ClassWrapper {
private:
	
	static std::map<v8::Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;
	
	// Common tasks to do for any new js object regardless of how it is created
	template<class BEHAVIOR>
	static void _initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object) {
	    js_object->SetInternalField(0, v8::External::New(isolate, (void*) cpp_object));
		
		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));
		
		// set up a callback so we can clean up our internal object when the javascript
		//   object is garbage collected
		auto callback_data = new SetWeakCallbackParameter();
		callback_data->cpp_object = cpp_object;
		callback_data->javascript_object.Reset(isolate, js_object);
		// callback_data->behavior = std::make_unique<typename std::enable_if<std::is_base_of<DestructorBehavior<T>,BEHAVIOR>::value, BEHAVIOR>::type>();

		// this guarantees BEHAVIOR is a subclass of DestructorBehavior or operator= will fail to compile
		callback_data->behavior = std::make_unique<BEHAVIOR>();

		callback_data->javascript_object.SetWeak(callback_data, V8ClassWrapper<T>::v8_destructor);
	}
	
	// users of the library should call get_instance, not the constructor directly
	V8ClassWrapper(v8::Isolate * isolate) : isolate(isolate) {
		// this is a bit weird and there's probably a better way, but create an unbound functiontemplate for use
		//   when wrapping an object that is created outside javascript when there is no way to create that
		//   class type directly from javascript (no js constructor)
		auto constructor_template = v8::FunctionTemplate::New(isolate);
		constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
		this->constructor_templates.push_back(constructor_template);
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
		// printf("isolate to wrapper map %p size: %d\n", &isolate_to_wrapper_map, (int)isolate_to_wrapper_map.size());
		if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
			auto new_object = new V8ClassWrapper<T>(isolate);
			isolate_to_wrapper_map.insert(std::make_pair(isolate, new_object));
			// printf("Creating instance %p for isolate: %p\n", new_object, isolate);
		}
		// printf("(after) isoate to wrapper map size: %d\n", (int)isolate_to_wrapper_map.size());
		
		auto object = isolate_to_wrapper_map[isolate];
		// printf("Returning v8 wrapper: %p\n", object);
		return *object;
	}
	
	/**
	* V8ClassWrapper objects shouldn't be deleted during the normal flow of your program unless the associated isolate
	*   is going away forever.   Things will break otherwise as no additional objects will be able to be created
	*   even though V8 will still present the ability to your javascript (I think)
	*/
	virtual ~V8ClassWrapper(){

		// this was happening when it wasn't supposed to, like when making temp copies.   need to disable copying or something
		//   if this line is to be added back
		// isolate_to_wrapper_map.erase(this->isolate);
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
	template<class BEHAVIOR>
	v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object) {
		auto isolate = this->isolate;
		// fprintf(stderr, "Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p\n", existing_cpp_object, this, isolate);
		
		v8::Isolate::Scope is(isolate);
		v8::Context::Scope cs(context);
		
		auto new_javascript_object = this->constructor_templates[0]->InstanceTemplate()->NewInstance();
		_initialize_new_js_object<BEHAVIOR>(isolate, new_javascript_object, existing_cpp_object);
		return new_javascript_object;
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
			               const v8::PropertyCallbackInfo<v8::Value>& info);

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
		
		// typedef MEMBER_TYPE T::*MEMBER_POINTER_TYPE;

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
			
			CallerQualifierRemover<METHOD_TYPE>()(method, *backing_object_pointer, info);
			
		});
		
		auto function_template = v8::FunctionTemplate::New(this->isolate, callback_helper, v8::External::New(this->isolate, f));
		
		for(auto constructor_template : this->constructor_templates) {
			constructor_template->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		}
		return *this;
	}
	
	struct SetWeakCallbackParameter {
		T * cpp_object;
		v8::Global<v8::Object> javascript_object;
		std::unique_ptr<DestructorBehavior<T>> behavior;
	};
	
	
	
	// Helper for creating objects when "new MyClass" is called from java
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
		
		T * new_cpp_object = call_cpp_constructor<CONSTRUCTOR_PARAMETER_TYPES...>(args, std::index_sequence_for<CONSTRUCTOR_PARAMETER_TYPES...>());
		_initialize_new_js_object<DestructorBehaviorLeaveAlone<T>>(isolate, args.This(), new_cpp_object);
		
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
		(*parameter->behavior)(parameter->cpp_object);

		isolate->AdjustAmountOfExternalAllocatedMemory(-sizeof(T));

		// Clear out the Global<Object> handle
		parameter->javascript_object.Reset();

		// Delete the heap-allocated std::pair from v8_constructor
		delete parameter;
	}
};

template <class T> std::map<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T>::isolate_to_wrapper_map;







// casts from a primitive to a v8::Value
template<typename T>
struct CastToJS {
	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T & cpp_object){
		return CastToJS<T*>()(isolate, &cpp_object);
	}
	
	// If an rvalue is passed in, a copy must be made.
	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T && cpp_object){
		printf("Asked to convert rvalue type, so copying it first\n");

		// this memory will be owned by the javascript object and cleaned up if/when the GC removes the object
		auto copy = new T(cpp_object);
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return class_wrapper.template wrap_existing_cpp_object<DestructorBehaviorDelete<T>>(context, copy);
	}
};


template<typename CLASS_TYPE, typename METHOD_TYPE, typename ... PARAMETERS>
void run_non_void(CLASS_TYPE & object, METHOD_TYPE method, const v8::FunctionCallbackInfo<v8::Value> & info, PARAMETERS... parameters) {
	// decltype((object.*method)(parameters...)) return_value = (object.*method)(parameters...);
	auto casted_result = CastToJS<decltype((object.*method)(parameters...))>()(info.GetIsolate(), (object.*method)(parameters...));
	info.GetReturnValue().Set(casted_result);
}


template<class T>
template<class VALUE_T>
void V8ClassWrapper<T>::GetterHelper(v8::Local<v8::String> property,
		               const v8::PropertyCallbackInfo<v8::Value>& info) {
	v8::Local<v8::Object> self = info.Holder();				   
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	T * cpp_object = static_cast<T *>(wrap->Value());

	auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
	auto & member_ref = (*member_reference_getter)(cpp_object);
	info.GetReturnValue().Set(CastToJS<VALUE_T>()(info.GetIsolate(), member_ref));
}


#include "casts.hpp"



#include <boost/format.hpp>
static void print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline) {

	if (args.Length() > 0) {
		auto string = *v8::String::Utf8Value(args[0]);
		auto format = boost::format(string);
		
		for (int i = 1; i < args.Length(); i++) {
			format % *v8::String::Utf8Value(args[i]);
		}
		std::cout << format;	
		if (append_newline) {
			std::cout << std::endl;
		}
	}
}



static void print_callback(const v8::FunctionCallbackInfo<v8::Value>& args) {
	return print_helper(args, false);
}

static void println_callback(const v8::FunctionCallbackInfo<v8::Value>& args) {
	return print_helper(args, true);
}



void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> global_template )
{
	v8::HandleScope hs(isolate);
	auto print_template = v8::FunctionTemplate::New(isolate, &print_callback);
	global_template->Set(isolate, "print", print_template);
	
	auto println_template = v8::FunctionTemplate::New(isolate, &println_callback);
	global_template->Set(isolate, "println", println_template);
	
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


