



#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <functional>
#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <assert.h>

#include "v8toolkit.hpp"

namespace v8toolkit {

#define DEBUG false

/**
* Design Questions:
* - When a c++ object returns a new object represented by one of its members, should it
*   return the same javascript object each time as well?  
*     class Thing {
*       OtherClass other_class;
*       OtherClass & get_other_class(){return this->other_class;} <== should this return the same javascript object on each call for the same Thing object
*     }
*   - that's currently what the existing_wrapped_objects map is for, but if a new object
*     of the same time is created at the same address as an old one, the old javascript 
*     object will be returned.
*     - how would you track if the c++ object source object for an object went away?
*     - how would you actually GC the old object when containing object went away?
*   - Maybe allow some type of optional class customization to help give hints to V8ClassWrapper to have better behavior
*
*/


/***
* set of classes for determining what to do do the underlying c++
*   object when the javascript object is garbage collected
*/
template<class T>
struct DestructorBehavior 
{
	virtual void operator()(v8::Isolate * isolate, T* object) const = 0;
};


template<class T>
struct DestructorBehaviorDelete : DestructorBehavior<T> 
{
	void operator()(v8::Isolate * isolate, T* object) const 
	{
		if (DEBUG) printf("Deleting object at %p during V8 garbage collection\n", object);
		delete object;
		isolate->AdjustAmountOfExternalAllocatedMemory(-sizeof(T));
	}
};


template<class T>
struct DestructorBehaviorLeaveAlone : DestructorBehavior<T> 
{
	void operator()(v8::Isolate * isolate, T* object) const 
	{
		if (DEBUG) printf("Not deleting object %p during V8 garbage collection\n", object);
	}
};


/**
* Provides a mechanism for creating javascript-ready objects from an arbitrary C++ class
* Can provide a JS constructor method or wrap objects created in another c++ function
*/
template<class T>
class V8ClassWrapper 
{
private:
	
	static std::map<v8::Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;
	
	// Common tasks to do for any new js object regardless of how it is created
	template<class BEHAVIOR>
	static void _initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object) 
	{
	    js_object->SetInternalField(0, v8::External::New(isolate, (void*) cpp_object));
		
		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));
		
		v8toolkit::global_set_weak(isolate, js_object, [isolate, cpp_object]() {
				BEHAVIOR()(isolate, cpp_object);
			}
		);
	}
	
	// users of the library should call get_instance, not the constructor directly
	V8ClassWrapper(v8::Isolate * isolate) : isolate(isolate) 
	{
		// this is a bit weird and there's probably a better way, but create an unbound functiontemplate for use
		//   when wrapping an object that is created outside javascript when there is no way to create that
		//   class type directly from javascript (no js constructor)
		auto constructor_template = v8::FunctionTemplate::New(isolate);
		constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
		this->constructor_templates.push_back(constructor_template);
	}
	
	template<class VALUE_T> // type being returned
	static void _getter_helper(v8::Local<v8::String> property,
	                  const v8::PropertyCallbackInfo<v8::Value>& info) 
	{
						   
		v8::Local<v8::Object> self = info.Holder();				   
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());

		// This function returns a reference to member in question
		auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
	
		auto & member_ref = (*member_reference_getter)(cpp_object);
		info.GetReturnValue().Set(CastToJS<VALUE_T>()(info.GetIsolate(), member_ref));
	}

	template<typename VALUE_T>
	static void _setter_helper(v8::Local<v8::String> property, v8::Local<v8::Value> value,
	               const v8::PropertyCallbackInfo<void>& info) 
	{
		v8::Local<v8::Object> self = info.Holder();				   
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());

		auto member_reference_getter = (std::function<VALUE_T&(T*)> *)v8::External::Cast(*(info.Data()))->Value();
		auto & member_ref = (*member_reference_getter)(cpp_object);
	  	member_ref = CastToNative<VALUE_T>()(value);
		auto & m2 = (*member_reference_getter)(cpp_object);
	}
	
	// Helper for creating objects when "new MyClass" is called from java
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
		
		T * new_cpp_object = call_cpp_constructor<CONSTRUCTOR_PARAMETER_TYPES...>(args, std::index_sequence_for<CONSTRUCTOR_PARAMETER_TYPES...>());
		if (DEBUG) printf("In v8_constructor and created new cpp object at %p\n", new_cpp_object);

		// if the object was created by calling new in javascript, it should be deleted when the garbage collector 
		//   GC's the javascript object, there should be no c++ references to it
		_initialize_new_js_object<DestructorBehaviorDelete<T>>(isolate, args.This(), new_cpp_object);
		
		// return the object to the javascript caller
		args.GetReturnValue().Set(args.This());
	}


	template <typename ...Fs, size_t...ns> 
	static T * call_cpp_constructor(const v8::FunctionCallbackInfo<v8::Value> & args, std::index_sequence<ns...>){
		return new T(CastToNative<Fs>()(args[ns])...);
	}
	
	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args) 
	{
		StdFunctionCallbackType * callback_lambda = (StdFunctionCallbackType *)v8::External::Cast(*(args.Data()))->Value();		
		(*callback_lambda)(args);
	}
	
	// data to pass into a setweak callback
	struct SetWeakCallbackParameter 
	{
		T * cpp_object;
		v8::Global<v8::Object> javascript_object;
		std::unique_ptr<DestructorBehavior<T>> behavior;
	};
	
	
	
	// Helper for cleaning up the underlying wrapped c++ object when the corresponding javascript object is
	// garbage collected
	static void v8_destructor(const v8::WeakCallbackData<v8::Object, SetWeakCallbackParameter> & data) {
		auto isolate = data.GetIsolate();

		SetWeakCallbackParameter * parameter = data.GetParameter();
		
		// delete our internal c++ object and tell V8 the memory has been removed
		(*parameter->behavior)(isolate, parameter->cpp_object);
		
		// TODO: Need to remove object from existing_wrapped_objects hash properly resetting the global 
		
		
		// Clear out the Global<Object> handle
		parameter->javascript_object.Reset();

		// Delete the heap-allocated std::pair from v8_constructor
		delete parameter;
	}

	// these are tightly tied, as a FunctionTemplate is only valid in the isolate it was created with
	std::vector<v8::Local<v8::FunctionTemplate>> constructor_templates; // TODO: THIS CANNOT BE A LOCAL (most likely)
	std::map<T *, v8::Global<v8::Object>> existing_wrapped_objects; // TODO: This can't be a strong reference global or the object will never be GC'd
	v8::Isolate * isolate;
	bool member_or_method_added = false;
	
public:
	
	
	/**
	* Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
	* For each isolate you need to add constructors/methods/members separately.
	*/
	static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate) 
	{
		if (DEBUG) printf("isolate to wrapper map %p size: %d\n", &isolate_to_wrapper_map, (int)isolate_to_wrapper_map.size());
		if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
			auto new_object = new V8ClassWrapper<T>(isolate);
			isolate_to_wrapper_map.insert(std::make_pair(isolate, new_object));
			if (DEBUG) printf("Creating instance %p for isolate: %p\n", new_object, isolate);
		}
		if (DEBUG) printf("(after) isoate to wrapper map size: %d\n", (int)isolate_to_wrapper_map.size());
		
		auto object = isolate_to_wrapper_map[isolate];
		if (DEBUG) printf("Returning v8 wrapper: %p\n", object);
		return *object;
	}
	
	/**
	* V8ClassWrapper objects shouldn't be deleted during the normal flow of your program unless the associated isolate
	*   is going away forever.   Things will break otherwise as no additional objects will be able to be created
	*   even though V8 will still present the ability to your javascript (I think)
	*/
	virtual ~V8ClassWrapper()
	{
		// this was happening when it wasn't supposed to, like when making temp copies.   need to disable copying or something
		//   if this line is to be added back
		// isolate_to_wrapper_map.erase(this->isolate);
	}
	
	/**
	* Creates a javascript method of the specified name which, when called with the "new" keyword, will return
	*   a new object of this type
	*/
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	V8ClassWrapper<T> & add_constructor(std::string js_constructor_name, v8::Local<v8::ObjectTemplate> & parent_template) 
	{
				
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
	v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object) 
	{
		auto isolate = this->isolate;
		if (DEBUG) printf("Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p\n", existing_cpp_object, this, isolate);
		
		
		// if there's currently a javascript object wrapping this pointer, return that instead of making a new one
		v8::Local<v8::Object> javascript_object;
		if(this->existing_wrapped_objects.find(existing_cpp_object) != this->existing_wrapped_objects.end()) {
			if (DEBUG) printf("Found existing javascript object for c++ object %p\n", existing_cpp_object);
			javascript_object = v8::Local<v8::Object>::New(isolate, this->existing_wrapped_objects[existing_cpp_object]);
			
		} else {
		
			if (DEBUG) printf("Creating new javascript object for c++ object %p\n", existing_cpp_object);
		
			v8::Isolate::Scope is(isolate);
			v8::Context::Scope cs(context);
		
			javascript_object = this->constructor_templates[0]->InstanceTemplate()->NewInstance();
			_initialize_new_js_object<BEHAVIOR>(isolate, javascript_object, existing_cpp_object);
			
			this->existing_wrapped_objects.insert(std::pair<T*, v8::Global<v8::Object>>(existing_cpp_object, v8::Global<v8::Object>(isolate, javascript_object)));
			if (DEBUG) printf("Inserting new object into existing_wrapped_objects hash that is now of size: %d\n", (int)this->existing_wrapped_objects.size());			
		}
		return javascript_object;
	}


	typedef std::function<void(const v8::FunctionCallbackInfo<v8::Value>& info)> StdFunctionCallbackType;


	/**
	* Adds a getter and setter method for the specified class member
	* add_member(&ClassName::member_name, "javascript_attribute_name");
	*/
	template<typename MEMBER_TYPE>
	V8ClassWrapper<T> & add_member(MEMBER_TYPE T::* member, std::string member_name) 
	{

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
				_getter_helper<MEMBER_TYPE>, 
				_setter_helper<MEMBER_TYPE>, 
				v8::External::New(isolate, &get_member_reference));
		}
		return *this;
	}


	/**
	* adds the ability to call the specified class instance method on an object of this type
	* add_method(&ClassName::method_name, "javascript_attribute_name");
	*/
	template<class METHOD_TYPE>
	V8ClassWrapper<T> & add_method(METHOD_TYPE method, std::string method_name)
	{
		
		// stop additional constructors from being added
		member_or_method_added = true;
		
		// this is leaked if this ever isn't used anymore
		StdFunctionCallbackType * f = new StdFunctionCallbackType([method](const v8::FunctionCallbackInfo<v8::Value>& info) 
		{
			// get the behind-the-scenes c++ object
			auto self = info.Holder();
			auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
			void* ptr = wrap->Value();
			auto backing_object_pointer = static_cast<T*>(ptr);
			
			// bind the object and method into a std::function then build the parameters for it and call it
			auto bound_method = v8toolkit::bind(*backing_object_pointer, method);
			v8toolkit::ParameterBuilder<0, decltype(bound_method), decltype(bound_method)>()(bound_method, info);			
		});
		
		auto function_template = v8::FunctionTemplate::New(this->isolate, callback_helper, v8::External::New(this->isolate, f));
		
		for(auto constructor_template : this->constructor_templates) {
			constructor_template->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		}
		return *this;
	}	
};

/**
* Stores the "singleton" per isolate
*/
template <class T> std::map<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T>::isolate_to_wrapper_map;

template<typename T>
struct CastToJS {

	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T & cpp_object){
		if (DEBUG) printf("In base cast to js struct with lvalue ref\n");
		return CastToJS<T*>()(isolate, &cpp_object);
	}

	/**
	* If an rvalue is passed in, a copy must be made.
	*/
	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T && cpp_object){
		if (DEBUG) printf("In base cast to js struct with rvalue ref");
		if (DEBUG) printf("Asked to convert rvalue type, so copying it first\n");

		// this memory will be owned by the javascript object and cleaned up if/when the GC removes the object
		auto copy = new T(cpp_object);
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return class_wrapper.template wrap_existing_cpp_object<DestructorBehaviorDelete<T>>(context, copy);
	}
};

/**
* Attempt to use V8ClassWrapper to wrap any remaining types not handled by the specializations in casts.hpp
* That type must have had its methods and members added beforehand in the same isolate
*/
template<typename T>
struct CastToJS<T*> {
	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T * cpp_object){
		if (DEBUG) printf("CastToJS from T*\n");
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return class_wrapper.template wrap_existing_cpp_object<DestructorBehaviorLeaveAlone<T>>(context, cpp_object);
	}
};

template<typename T>
struct CastToJS<T&> {
	v8::Local<v8::Object> operator()(v8::Isolate * isolate, T & cpp_object){
		return CastToJS<T*>()(isolate, &cpp_object);		
	}
};

template<typename T>
struct CastToNative
{
	// implementation for above struct's operator()
	T & operator()(v8::Local<v8::Value> & value){
		if (DEBUG) printf("cast to native\n");
		auto object = v8::Object::Cast(*value);
		assert(object->InternalFieldCount() > 0);
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
		T * cpp_object = static_cast<T *>(wrap->Value());
		return *cpp_object;
	}
};

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


}