#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

#include <functional>
#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <assert.h>

#include "v8toolkit.h"

namespace v8toolkit {

#define V8_CLASS_WRAPPER_DEBUG false

class CastException : public std::exception {
private:
  std::string reason;
  
public:
    CastException(const std::string & reason) : reason(reason) {}
    virtual const char * what() const noexcept override {return reason.c_str();}
};

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


/*
    How to add static methods to every object as it is created?  You can add them by hand afterwards
    with v8toolkit::add_function, but there should be a way in v8classwrapper to say every object
    of the type gets the method, too
*/

template <bool... b> struct static_all_of;

//implementation: recurse, if the first argument is true
template <bool... tail> 
struct static_all_of<true, tail...> : static_all_of<tail...> {};

//end recursion if first argument is false - 
template <bool... tail> 
struct static_all_of<false, tail...> : std::false_type {};

// - or if no more arguments
template <> struct static_all_of<> : std::true_type {};


/***
* set of classes for determining what to do do the underlying c++
*   object when the javascript object is garbage collected
*/
template<class T>
struct DestructorBehavior 
{
	virtual void operator()(v8::Isolate * isolate, T* object) const = 0;
};


/**
* Helper to delete a C++ object when the corresponding javascript object is garbage collected
*/
template<class T>
struct DestructorBehaviorDelete : DestructorBehavior<T> 
{
	void operator()(v8::Isolate * isolate, T* object) const 
	{
		if (V8_CLASS_WRAPPER_DEBUG) printf("Deleting object at %p during V8 garbage collection\n", object);
		delete object;
		isolate->AdjustAmountOfExternalAllocatedMemory(-sizeof(T));
	}
};

/**
* Helper to not do anything to the underlying C++ object when the corresponding javascript object
*   is garbage collected
*/
template<class T>
struct DestructorBehaviorLeaveAlone : DestructorBehavior<T> 
{
	void operator()(v8::Isolate * isolate, T* object) const 
	{
		if (V8_CLASS_WRAPPER_DEBUG) printf("Not deleting object %p during V8 garbage collection\n", object);
	}
};






template<class T>
struct TypeCheckerBase {
  public:
      virtual ~TypeCheckerBase(){}
      virtual T * check(AnyBase *) = 0;
};

template<class, class...>
struct TypeChecker;

template<class T, class Head>
struct TypeChecker<T, Head> : public TypeCheckerBase<T>
{
    T * check(AnyBase * any_base) {
        Any<Head> * any = nullptr;
        if((any = dynamic_cast<Any<Head> *>(any_base)) != nullptr) {
            return static_cast<T*>(any->get());
        } else {
            return nullptr;
        }
    }
};

template<class T, class Head, class... Tail>
struct TypeChecker<T, Head, Tail...> : public TypeChecker<T, Tail...> {
    using SUPER = TypeChecker<T, Tail...>;
    T * check(AnyBase * any_base) {
        Any<Head> * any = nullptr;
        if((any = dynamic_cast<Any<Head> *>(any_base)) != nullptr) {
            return static_cast<T*>(any->get());
        } else {
            return SUPER::check(any_base);
        }
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
	V8ClassWrapper<T>() = delete;
	V8ClassWrapper<T>(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper<T>(const V8ClassWrapper<T> &&) = delete;	
	V8ClassWrapper<T>& operator=(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper<T>& operator=(const V8ClassWrapper<T> &&) = delete;
	void foo() = delete;
	
	// Common tasks to do for any new js object regardless of how it is created
	template<class BEHAVIOR>
	static void _initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object) 
	{
        auto any = new Any<T>(cpp_object);
	    js_object->SetInternalField(0, v8::External::New(isolate, static_cast<AnyBase*>(any)));
		
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
		v8toolkit::scoped_run(isolate, [this](auto isolate) {
			auto constructor_template = v8::FunctionTemplate::New(isolate);
			constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
			this->constructor_templates.emplace_back(v8::Global<v8::FunctionTemplate>(isolate, constructor_template));
		});
	}
	
    // function used to return the value of a C++ variable backing a javascript variable visible
    //   via the V8 SetAccessor method
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

    // function used to set the value of a C++ variable backing a javascript variable visible
    //   via the V8 SetAccessor method
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
		(*member_reference_getter)(cpp_object);
	}
    
	template <typename... Fs, size_t... ns> 
	static T * call_cpp_constructor(const v8::FunctionCallbackInfo<v8::Value> & args, std::index_sequence<ns...>){
		return new T(CastToNative<Fs>()(args[ns])...);
	}
    
	
	// Helper for creating objects when "new MyClass" is called from java
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
		
		T * new_cpp_object = call_cpp_constructor<CONSTRUCTOR_PARAMETER_TYPES...>(args, std::index_sequence_for<CONSTRUCTOR_PARAMETER_TYPES...>());
		if (V8_CLASS_WRAPPER_DEBUG) printf("In v8_constructor and created new cpp object at %p\n", new_cpp_object);

		// if the object was created by calling new in javascript, it should be deleted when the garbage collector 
		//   GC's the javascript object, there should be no c++ references to it
		_initialize_new_js_object<DestructorBehaviorDelete<T>>(isolate, args.This(), new_cpp_object);
		
		// return the object to the javascript caller
		args.GetReturnValue().Set(args.This());
	}
	
	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args) 
	{
		StdFunctionCallbackType * callback_lambda = (StdFunctionCallbackType *)v8::External::Cast(*(args.Data()))->Value();		
		(*callback_lambda)(args);
	}

	// these are tightly tied, as a FunctionTemplate is only valid in the isolate it was created with
	std::vector<v8::Global<v8::FunctionTemplate>> constructor_templates;
	std::map<T *, v8::Global<v8::Object>> existing_wrapped_objects;
	v8::Isolate * isolate;

    static std::unique_ptr<TypeCheckerBase<T>> type_checker;
    
    // this is to signal undesirable usage patterns where methods or members won't be visible to 
    //   objects created in certain ways
	bool member_or_method_added = false;
	
public:
	
    static T * cast(AnyBase * any_base)
    {
        if(type_checker != nullptr) {
            return type_checker->check(any_base);
        } else if (dynamic_cast<Any<T>*>(any_base)) {
                return static_cast<Any<T>*>(any_base)->get();
        }
        return nullptr;
         
    }
	
	/**
	* Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
	* For each isolate you need to add constructors/methods/members separately.
	*/
	static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate) 
	{
		if (V8_CLASS_WRAPPER_DEBUG) printf("isolate to wrapper map %p size: %d\n", &isolate_to_wrapper_map, (int)isolate_to_wrapper_map.size());
		if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
			auto new_object = new V8ClassWrapper<T>(isolate);
			isolate_to_wrapper_map.insert(std::make_pair(isolate, new_object));
			if (V8_CLASS_WRAPPER_DEBUG) printf("Creating instance %p for isolate: %p\n", new_object, isolate);
		}
		if (V8_CLASS_WRAPPER_DEBUG) printf("(after) isoate to wrapper map size: %d\n", (int)isolate_to_wrapper_map.size());
		
		auto object = isolate_to_wrapper_map[isolate];
		if (V8_CLASS_WRAPPER_DEBUG) printf("Returning v8 wrapper: %p\n", object);
		return *object;
	}


    /**
    * Species other types that can be substituted for T when calling a function expecting T
    *   but T is not being passsed.   Only available for classes derived from T.
    * T is always compatible and should not be specified here.
    * Not calling this means that only T objects will be accepted for things that want a T.
    * There is no automatic determination of inherited types by this library because I cannot
    *   figure out how.
    */
    template<class... CompatibleTypes>
    static
    std::enable_if_t<static_all_of<std::is_base_of<T,CompatibleTypes>::value...>::value>
    set_compatible_types()
    {
        type_checker.reset(new TypeChecker<T, T, CompatibleTypes...>());
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
	* Creates a javascript method with the specified name which, when called with the "new" keyword, will return
	*   a new object of this type.
	*/
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	V8ClassWrapper<T> & add_constructor(std::string js_constructor_name, v8::Local<v8::ObjectTemplate> parent_template) 
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
		this->constructor_templates.push_back(v8::Global<v8::FunctionTemplate>(isolate, constructor_template));
				
				
		// Add the constructor function to the parent object template (often the global template)
		parent_template->Set(v8::String::NewFromUtf8(isolate, js_constructor_name.c_str()), constructor_template);
				
		return *this;
	}
	
	// Not sure if this properly sets the prototype of the new object like when the constructor functiontemplate is called as
	//   a constructor from javascript
	/**
	* Used when wanting to return an object from a c++ function call back to javascript, or in conjunction with
    *   add_variable to give a javascript name to an existing c++ object 
    * \code{cpp}
    * add_variable(context, context->GetGlobal(), "js_name", class_wrapper.wrap_existing_cpp_object(context, some_c++_object));
    * \endcode
	*/
	template<class BEHAVIOR>
	v8::Local<v8::Value> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object) 
	{
		auto isolate = this->isolate;
		if (V8_CLASS_WRAPPER_DEBUG) printf("Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p\n", existing_cpp_object, this, isolate);
		
		
		// if there's currently a javascript object wrapping this pointer, return that instead of making a new one
		v8::Local<v8::Object> javascript_object;
		if(this->existing_wrapped_objects.find(existing_cpp_object) != this->existing_wrapped_objects.end()) {
			if (V8_CLASS_WRAPPER_DEBUG) printf("Found existing javascript object for c++ object %p\n", existing_cpp_object);
			javascript_object = v8::Local<v8::Object>::New(isolate, this->existing_wrapped_objects[existing_cpp_object]);
			
		} else {
		
			if (V8_CLASS_WRAPPER_DEBUG) printf("Creating new javascript object for c++ object %p\n", existing_cpp_object);
		
			v8::Isolate::Scope is(isolate);
			v8::Context::Scope cs(context);
		
			javascript_object = this->constructor_templates[0].Get(isolate)->InstanceTemplate()->NewInstance();
			_initialize_new_js_object<BEHAVIOR>(isolate, javascript_object, existing_cpp_object);
			
			// this->existing_wrapped_objects.emplace(existing_cpp_object, v8::Global<v8::Object>(isolate, javascript_object));
			if (V8_CLASS_WRAPPER_DEBUG) printf("Inserting new object into existing_wrapped_objects hash that is now of size: %d\n", (int)this->existing_wrapped_objects.size());			
		}
		return v8::Local<v8::Value>::Cast(javascript_object);
		// return javascript_object;
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
		
		// this lambda is shared between the getter and the setter so it can only do work needed by both
        // TODO: this is leaked if the classwrapper ever goes away (which it shouldn't), but it's still not right to do
		auto get_member_reference = new std::function<MEMBER_TYPE&(T*)>([member](T * cpp_object)->MEMBER_TYPE&{
			return cpp_object->*member;
		});
        
		for(auto & constructor_template : this->constructor_templates) {
			constructor_template.Get(isolate)->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(isolate, 
				member_name.c_str()), 
				_getter_helper<MEMBER_TYPE>, 
				_setter_helper<MEMBER_TYPE>, 
				v8::External::New(isolate, get_member_reference));
		}
		return *this;
	}


	template<class R, class... Args>
	V8ClassWrapper<T> & add_method(R(T::*method)(Args...) const, std::string method_name) {
		return _add_method(method, method_name);
	}
    

	/**
	* Adds the ability to call the specified class instance method on an object of this type
	*/
	template<class R, class... Args>
	V8ClassWrapper<T> & add_method(R(T::*method)(Args...), std::string method_name)
	{
		return _add_method(method, method_name);
    }
    
    template<class M>
    V8ClassWrapper<T> & _add_method(M method, std::string method_name)
    {
		// stop additional constructors from being added
		member_or_method_added = true;
		
		// this is leaked if this ever isn't used anymore
		StdFunctionCallbackType * f = new StdFunctionCallbackType([method](const v8::FunctionCallbackInfo<v8::Value>& info) 
		{
            auto isolate = info.GetIsolate();
            
			// get the behind-the-scenes c++ object
			auto self = info.Holder();
			auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
			void* ptr = wrap->Value();
			auto backing_object_pointer = static_cast<T*>(ptr);
			
			// bind the object and method into a std::function then build the parameters for it and call it
			auto bound_method = v8toolkit::bind(*backing_object_pointer, method);
            
            using PB_TYPE = v8toolkit::ParameterBuilder<0, decltype(bound_method), decltype(bound_method)>;
            
            PB_TYPE pb;
            auto arity = PB_TYPE::ARITY;
            if(info.Length() < arity) {
                std::stringstream ss;
                ss << "Function called from javascript with insufficient parameters.  Requires " << arity << " provided " << info.Length();
                isolate->ThrowException(v8::String::NewFromUtf8(isolate, ss.str().c_str()));
                return; // return now so the exception can be thrown inside the javascript
            }
            
            // V8 does not support C++ exceptions, so all exceptions must be caught before control
            //   is returned to V8 or the program will instantly terminate
            try {
    			pb(bound_method, info);
            } catch(std::exception & e) {
                isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
                return;
            }
            return;
		});
		
		auto function_template = v8::FunctionTemplate::New(this->isolate, callback_helper, v8::External::New(this->isolate, f));
		
		for(auto & constructor_template : this->constructor_templates) {
			constructor_template.Get(isolate)->InstanceTemplate()->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		}
		return *this;
	}
};

/**
* Stores the "singleton" per isolate
*/
template <class T> 
std::map<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T>::isolate_to_wrapper_map;

template<class T>
std::unique_ptr<TypeCheckerBase<T>> V8ClassWrapper<T>::type_checker = std::make_unique<TypeChecker<T, T>>();


template<typename T>
struct CastToJS {

	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object){
		if (V8_CLASS_WRAPPER_DEBUG) printf("In base cast to js struct with lvalue ref\n");
		return CastToJS<T*>()(isolate, &cpp_object);
	}

	/**
	* If an rvalue is passed in, a copy must be made.
	*/
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object){
		if (V8_CLASS_WRAPPER_DEBUG) printf("In base cast to js struct with rvalue ref");
		if (V8_CLASS_WRAPPER_DEBUG) printf("Asked to convert rvalue type, so copying it first\n");

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
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){
		if (V8_CLASS_WRAPPER_DEBUG) printf("CastToJS from T*\n");
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return class_wrapper.template wrap_existing_cpp_object<DestructorBehaviorLeaveAlone<T>>(context, cpp_object);
	}
};

template<typename T>
struct CastToJS<T&> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object){
		return CastToJS<T*>()(isolate, &cpp_object);		
	}
};


template<typename T>
struct CastToNative<T*>
{
	T * operator()(v8::Local<v8::Value> value){
        return & CastToNative<T>()(value);
    }
};


template<typename T>
struct CastToNative
{
	T & operator()(v8::Local<v8::Value> value){
		if (V8_CLASS_WRAPPER_DEBUG) printf("cast to native\n");
        if(!value->IsObject()){
            throw CastException("No specialized CastToNative found and value was not a Javascript Object");
        }
		auto object = v8::Object::Cast(*value);
		if (object->InternalFieldCount() <= 0) {
            throw CastException("No specialization CastToNative found and provided Object is not a wrapped C++ object.  It is a native Javascript Object");
        }
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
        
        // I don't know any way to determine if a type is
        auto any_base = (v8toolkit::AnyBase *)wrap->Value();
        T * t = nullptr;
        if ((t = V8ClassWrapper<T>::cast(any_base)) == nullptr) {
            throw CastException("Wrapped class isn't an exact match for the parameter type and using inherited types isn't supported");
        }
        
		return *t;
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