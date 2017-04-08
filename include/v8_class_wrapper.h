#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <algorithm>

#include <iostream>
#include <vector>
#include <utility>
#include <assert.h>

// vector_map compiles a LOT faster than std::map as the number of wrapped classes increases
#define USE_EASTL_FOR_INTERNALS

#ifdef USE_EASTL_FOR_INTERNALS
#include <EASTL/vector_map.h>
template<class... ArgTs>
using MapT=eastl::vector_map<ArgTs...>;
#else
#include <map>
template<class... ArgTs>
using MapT=std::map<ArgTs...>;
#endif

#include "wrapped_class_base.h"
#include "v8toolkit.h"
#include "casts.hpp"

// allow _v8 suffix for making v8::String objects
using namespace v8toolkit::literals;

// this enables certain functionality only if bidirectional.h has been included
#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
#define V8_CLASS_WRAPPER_HAS_BIDIRECTIONAL_SUPPORT
#endif

namespace v8toolkit {


//#define V8_CLASS_WRAPPER_DEBUG

#ifdef V8_CLASS_WRAPPER_DEBUG
#define V8TOOLKIT_DEBUG(format_string, ...) \
    fprintf(stderr, format_string, ##__VA_ARGS__);
#else
#define V8TOOLKIT_DEBUG(format_string, ...)
#endif
/**
* Design Questions:
* - When a c++ object returns a new object represted by one of its members, should it
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
struct DestructorBehavior
{
	virtual void operator()(v8::Isolate * isolate, const void * object) const = 0;
};


/**
* Helper to delete a C++ object when the corresponding javascript object is garbage collected
*/
template<class T>
struct DestructorBehavior_Delete : DestructorBehavior {
	void operator()(v8::Isolate * isolate, const void * void_object) const {
		T* object = (T*)void_object;
        V8TOOLKIT_DEBUG("Deleting object at %p during V8 garbage collection\n", void_object);
		delete object;
		isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(sizeof(T)));
	}
};

/**
* Helper to not do anything to the underlying C++ object when the corresponding javascript object
*   is garbage collected
*/
struct DestructorBehavior_LeaveAlone : DestructorBehavior {
	void operator()(v8::Isolate * isolate, const void * void_object) const {
        V8TOOLKIT_DEBUG("Not deleting object %p during V8 garbage collection\n", void_object);
	}
};


// Helper struct which can determine if an embedded CPP object is compatible with
//   type T as well as casting an object of type T to its most derived type
template<class T>
struct TypeCheckerBase {
protected:
	v8::Isolate * isolate;

  public:
	TypeCheckerBase(v8::Isolate * isolate) : isolate(isolate) {}
	virtual ~TypeCheckerBase(){}

    // returns nullptr if AnyBase cannot be converted to a compatible type
    virtual T * check(AnyBase *, bool first_call = true
			) const = 0;
};

template<class T>
struct WrapAsMostDerivedBase {
protected:
	v8::Isolate * isolate;

public:
	WrapAsMostDerivedBase(v8::Isolate * isolate) : isolate(isolate) {}
	virtual ~WrapAsMostDerivedBase() = default;

	virtual v8::Local<v8::Object> operator()(T * cpp_object) const = 0;
	virtual v8::Local<v8::Object> operator()(T && cpp_object) const = 0;
};

// type to find the most derived type of an object and return a wrapped JavaScript object of that type
template<class, class, class = void>
struct WrapAsMostDerived;

template<class T>
struct WrapAsMostDerived<T, TypeList<>> : public WrapAsMostDerivedBase<T> {
	WrapAsMostDerived(v8::Isolate * isolate) : WrapAsMostDerivedBase<T>(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object) const override;
	virtual v8::Local<v8::Object> operator()(T && cpp_object) const override;
};


// specialization for when there is no const conflict
template<class T, class Head, class... Tail>
struct WrapAsMostDerived<T, TypeList<Head, Tail...>, std::enable_if_t<!std::is_const<T>::value || std::is_const<Head>::value>> : public WrapAsMostDerived<T, TypeList<Tail...>> {
	using SUPER = WrapAsMostDerived<T, TypeList<Tail...>>;
	WrapAsMostDerived(v8::Isolate * isolate) : SUPER(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object) const override;
	virtual v8::Local<v8::Object> operator()(T && cpp_object) const override;
};

// specializaiton for when we have something const (T) and want something non-const (Head)
template<class T, class Head, class... Tail>
struct WrapAsMostDerived<T, TypeList<Head, Tail...>, std::enable_if_t<std::is_const<T>::value && !std::is_const<Head>::value>> : public WrapAsMostDerived<T, TypeList<Tail...>> {
	using SUPER = WrapAsMostDerived<T, TypeList<Tail...>>;
	WrapAsMostDerived(v8::Isolate * isolate) : SUPER(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object) const override;
	virtual v8::Local<v8::Object> operator()(T && cpp_object) const override;
};




// type to convert to, typelist of all types to check, sfinae helper type
template<class, class, class = void>
struct TypeChecker;


// Fallback when everything in the typelist has been tried
template<class T>
    struct TypeChecker<T, TypeList<>> : public TypeCheckerBase<T>
{

		TypeChecker(v8::Isolate * isolate) : TypeCheckerBase<T>(isolate) {}
    virtual T * check(AnyBase * any_base, bool first_call = true
		      ) const override {
       ANYBASE_PRINT("Failed to find match for anybase ({}) with type string: {}", demangle<T>(), any_base->type_name);
        return nullptr;
    }
};


// Specialization for types that cannot possibly work -- casting const value to non-const return
template<class T, class Head, class... Tail>
struct TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,
    std::enable_if_t<!std::is_const<T>::value && std::is_const<Head>::value>> : public TypeChecker<T, TypeList<Tail...>> {

    using SUPER = TypeChecker<T, TypeList<Tail...>>;

	TypeChecker(v8::Isolate * isolate) : SUPER(isolate) {}

    virtual T * check(AnyBase * any_base, bool first_call = true) const override {
		ANYBASE_PRINT("In Type Checker<{}> Const mismatch: {} (string: {})", demangle<T>(), demangle<Head>(), any_base->type_name);
        if (dynamic_cast<AnyPtr<Head> *>(any_base) != nullptr) {
            ANYBASE_PRINT("Not a match, but the value is a const version of goal type! {} vs {} Should you be casting to a const type instead?", demangle<T>(), demangle<Head>());
        }
        return SUPER::check(any_base);
    }

};

 
// tests an AnyBase * against a list of types compatible with T
//   to see if the AnyBase is an Any<TypeList...> ihn
template<class T, class Head, class... Tail>
struct TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,

    // if it's *not* the condition of the specialization above
    std::enable_if_t<std::is_const<T>::value ||
					 !std::is_const<Head>::value>
	                > : public TypeChecker<T, TypeList<Tail...>> {


    using SUPER = TypeChecker<T, TypeList<Tail...>>;
	TypeChecker(v8::Isolate * isolate) : SUPER(isolate) {}

	virtual T * check(AnyBase * any_base, bool first_call = true) const override;

};





 // Cannot make class wrappers for pointer or reference types
#define V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE !std::is_pointer<T>::value && !std::is_reference<T>::value

#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX std::is_base_of<WrappedClassBase, T>::value
/**
 * Allows user to specify a list of types to instantiate real V8ClassWrapper template for -- CastToNative/CastToJS will otherwise
 *   try to instantiate it for a very large number of types which can drastically slow down compilation.
 * Setting an explicit value for this is NOT required - it is just a compile-time, compile-RAM (and maybe binary size) optimizatioan
 */
#ifdef V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE
#error this is no longer supported
#endif


#ifndef V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE
 class UnusedType;
#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE !std::is_same<UnusedType, T>::value
#endif


 // uncomment this to see the effects of generating the wrapper class on compile time (but won't actually run correctly)
// #define TEST_NO_REAL_WRAPPERS
 

 #ifdef TEST_NO_REAL_WRAPPERS
 class UnusedType;
#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<std::is_same<T, UnusedType>::value>
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<!std::is_same<T, UnusedType>::value>

 #else
// Use the real V8ClassWrapper specialization if the class inherits from WrappedClassBase or is in the user-provided sfinae
#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    ((V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX) || (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE))>

// otherwise use the 'cheap' specialization
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    !((V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX) || (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE))>
#endif



/**
 * Constructor names already used, including things reserved by JavaScript like "Object" and "Number"
 */
extern std::map<v8::Isolate *, std::vector<std::string>> used_constructor_name_list_map;

 
 template<class T, class = void> class V8ClassWrapper;


/**
 * The real template is quite expensive to make for types that don't need it,
 *   so here's an alternative for when it isn't actually going to be used
 */
 template<class T>
     class V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE> {
 public:

     static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate);

	 v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context,
                                                    T * existing_cpp_object,
                                                    DestructorBehavior const & destructor_behavior,
                                                    bool force_wrap_this_type = false);

     T * cast(AnyBase * any_base);


     template<class... Args1, class Callable, class Tuple = std::tuple<>>
	 void add_method(std::string const &, Callable&&, Tuple&&=Tuple());


	template<typename ... CONSTRUCTOR_PARAMETER_TYPES, class DefaultArgsTuple = std::tuple<>>
	void add_constructor(const std::string & js_constructor_name,
						 v8::Local<v8::ObjectTemplate> parent_template,
						 DefaultArgsTuple const & default_args = DefaultArgsTuple());

    template<class Callable>
    void add_new_constructor_function_template_callback(Callable&&);

	void finalize(bool wrap_as_most_derived = false);

         template<class MemberType,
             class MemberClass,
             MemberType (MemberClass::*member)>
	V8ClassWrapper<T> & add_member(std::string const &);

         template<class MemberType,
             class MemberClass,
             MemberType (MemberClass::*member)>
         V8ClassWrapper<T> & add_member_readonly(std::string const &);


     template<class...>
    V8ClassWrapper<T>& set_compatible_types();

     template<class>
    V8ClassWrapper<T> & set_parent_type();
 
     
     template<class... Args>
	 V8ClassWrapper<T> & add_static_method(Args&&...);

     T * get_cpp_object(v8::Local<v8::Object> object);

     
 	void set_class_name(const std::string & name);


 	v8::Local<v8::FunctionTemplate> get_function_template();

	 template<class... Args>
	 v8::Local<v8::Object> wrap_as_most_derived(Args&&...);

     std::unique_ptr<DestructorBehavior> destructor_behavior_delete;
     std::unique_ptr<DestructorBehavior> destructor_behavior_leave_alone;

     static void initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object, DestructorBehavior const & destructor_behavior);
};



/**
* Provides a mechanism for creating javascript-ready objects from an arbitrary C++ class
* Can provide a JS constructor method or wrap objects created in another c++ function
*
* Const types should not be wrapped directly.   Instead, a const version of a non-const type will
* automatically be created and populated with read-only members and any const-qualified method added
* to the non-const version.
*
* All members/methods must be added, then finalize() called, then any desired constructors may be created.
*
*
*/ 
template<class T>
    
class V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>

{
	using ConstT = std::add_const_t<T>;
private:

	/*** TYPEDEFS ***/

    // Callback type to add members to an ObjectTemplate
	using AttributeAdder = func::function<void(v8::Local<v8::ObjectTemplate> &)>;

    // Callback type to add a static method to an ObjectTemplate
	using StaticMethodAdder = func::function<void(v8::Local<v8::FunctionTemplate>)>;

    // Callback type for notifying when a property has been changed
	using PropertyChangedCallback = func::function<void(v8::Isolate * isolate,
													   v8::Local<v8::Object> & self,
													   const std::string &,
													   const v8::Local<v8::Value> & value)>;

    // Callback type to add a FakeMethod to an ObjectTemplate
	using FakeMethodAdder = func::function<void(v8::Local<v8::ObjectTemplate>)>;

    // Callback type to add a method
	using FunctionTemplateCallback = func::function<void(v8::Local<v8::FunctionTemplate> &)>;



	/*** DATA MEMBERS ***/

    // Callbacks for adding members to an ObjectTemplate
	std::vector<AttributeAdder> member_adders;

    // Callbacks for adding static methods to an ObjectTemplate
	std::vector<StaticMethodAdder> static_method_adders;


	/// List of callbacks for when attributes change
	std::vector<PropertyChangedCallback> property_changed_callbacks;


	// stores callbacks to add calls to lambdas whos first parameter is of type T* and are automatically passed
	//   the "this" pointer before any javascript parameters are passed in
	std::vector<FakeMethodAdder> fake_method_adders;

	/**
	 * Name that will be reported from JavaScript `typeof` function
	 */
	std::string class_name = demangle<T>();

	/**
	 * List of names already in use for methods/static methods/accessors
	 * Used to make sure duplicate names aren't requested
	 */
    std::vector<std::string> used_attribute_name_list;

    /**
     * List of names already used for properties on the constructor function
     */
    std::vector<std::string> used_static_attribute_name_list;


    /**
     * Mapping between CPP object pointer and JavaScript object for CPP objects which have already been
     * wrapped so the previous wrapped object can be returned.
     * Note: this stops any objects from ever being deleted and can cause problems if another object of the same
     * type is created at the same memory address.
     */
	MapT<T *, v8::Global<v8::Object>> existing_wrapped_objects;

    /**
     * Isolate associated with this V8ClassWrapper
     */
	v8::Isolate * isolate;

	/**
	 * Wrapped classes are per-isolate, so this tracks each wrapped class/isolate tuple for later retrieval
	 */
	static MapT<v8::Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;


	// Stores a functor capable of converting compatible types into a <T> object
	// do non-const type first, so if it's a match, we don't try converting the non-const type to const and hit a debug assertion
	TypeCheckerBase<T> * type_checker = new TypeChecker<T, TypeList<std::remove_const_t<T>, std::add_const_t<T>>>(this->isolate); // std::unique_ptr adds too much compilation time
	WrapAsMostDerivedBase<T> * wrap_as_most_derived_object = new WrapAsMostDerived<T, TypeList<>>(this->isolate); // std::unique_ptr adds too much compilation time



    /****** METHODS *******/

	/**
	* Stores a function template with any methods from the parent already in place.
	* Used as the prototype for any new object
	*/
    v8::Global<v8::FunctionTemplate> global_parent_function_template;

    /**
    * Have to store all the function templates this class wrapper has ever made so
    *   they can all be tried as parameters to v8::Object::GetInstanceFromPrototypeChain
    */
    std::vector<v8::Global<v8::FunctionTemplate>> this_class_function_templates;



    /**
    * Forces user to state that all members/methods have been added before any
    *   instances of the wrapped object are created
    */
    bool finalized = false;

    /**
     * Whether the type should try to determine the most derived type of a CPP object or just wrap it as the
     * presented static type - takes longer to determine the most derived type, but may be necessary for having the
     * appropriate attributes on the returned JS object
     */
    bool wrap_as_most_derived_flag = false;

	// Nothing may ever be removed from this vector, as things point into it
	std::list<MethodAdderData> method_adders;

	// makes a single function to be run when the wrapping javascript object is called with ()
	MethodAdderData callable_adder;

	func::function<void(v8::Local<v8::ObjectTemplate>)> named_property_adder;
	v8::IndexedPropertyGetterCallback indexed_property_getter = nullptr;

	std::vector<FunctionTemplateCallback> function_template_callbacks;


	/*** METHODS ***/





	V8ClassWrapper() = delete;
	V8ClassWrapper(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper(const V8ClassWrapper<T> &&) = delete;
	V8ClassWrapper& operator=(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper& operator=(const V8ClassWrapper<T> &&) = delete;


	/**
	 * Private constructor that places the newly created object in the "singleton" map.
	 * Users of the library should call get_instance, not this constructor directly
	 * @param isolate isolate for which the class wrapper is for
	 */
	V8ClassWrapper(v8::Isolate * isolate);




	/**
	 * Calls registered callbacks when the specified property name is changed
	 * @param object JavaScript object on which the property is being changed
	 * @param property_name property name being changed
	 * @param value new value of property
	 */
	void call_callbacks(v8::Local<v8::Object> object, const std::string & property_name, v8::Local<v8::Value> & value);


	/**
	 * Checks to see if a name has already been used because the V8 error message for a duplicate name is not helpful
	 * @param name name to check
	 */
    void check_if_name_used(const std::string & name);


    /**
     * static methods go on the constructor function, so it can have names which overlap with the per-instance object attributes
     * @param name name of static method to check
     */
    void check_if_static_name_used(const std::string & name);


	/**
	 * returns whether the name is already used at a global level
	 * @param name name to check
	 */
	void check_if_constructor_name_used(std::string const &);


    // function used to return the value of a C++ variable backing a javascript variable visible
    //   via the V8 SetAccessor method
	template<class MemberType, class MemberClass, MemberType (MemberClass::*member_pointer)> // type being returned
	static void _getter_helper(v8::Local<v8::Name> property,
							   v8::PropertyCallbackInfo<v8::Value> const & info) {

		auto isolate = info.GetIsolate();

		auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());
        MemberType & value = cpp_object->*member_pointer;

        // add lvalue ref as to know not to delete the object if the JS object is garbage collected
        info.GetReturnValue().Set(CastToJS<std::add_lvalue_reference_t<MemberType &>>()(isolate, value));
    }


	/**
	 * Called when a JavaScript object's property is assigned to
	 * @param property property name
	 * @param value new value for property
	 * @param info general JavaScript state info
	 */
	template<class MemberType, class MemberClass, MemberType (MemberClass::*member_pointer),
		std::enable_if_t<std::is_copy_assignable<MemberType>::value, int> = 0>
	static void _setter_helper(v8::Local<v8::Name> property,
							   v8::Local<v8::Value> value,
							   v8::PropertyCallbackInfo<void> const & info) {
	    auto isolate = info.GetIsolate();

	    T * cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());
		cpp_object->*member_pointer = CastToNative<MemberType>()(isolate, value);

	    // call any registered change callbacks
	    V8ClassWrapper<T>::get_instance(isolate).call_callbacks(info.Holder(), *v8::String::Utf8Value(property), value);
	}


	/**
	 * Setter cannot do anything for types that aren't copy assignable
	 * @param property ignored
	 * @param value ignored
	 * @param info ignored
	 */
	template<typename MemberType, class MemberClass, MemberType (MemberClass::*member_pointer),
		std::enable_if_t<!std::is_copy_assignable<MemberType>::value, int> = 0>
	static void _setter_helper(v8::Local<v8::Name> property, v8::Local<v8::Value> value,
							   const v8::PropertyCallbackInfo<void>& info)
    {}


	// Helper for creating objects when "new MyClass" is called from javascript
	template<typename DefaultArgsTupleType, typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& info) {
		auto isolate = info.GetIsolate();

		// default arguments are in tuple
		DefaultArgsTupleType * default_args_tuple_ptr =
			static_cast<DefaultArgsTupleType *>(v8::Local<v8::External>::Cast(info.Data())->Value());

		T * new_cpp_object = nullptr;
		func::function<void(CONSTRUCTOR_PARAMETER_TYPES...)> constructor =
				[&new_cpp_object](CONSTRUCTOR_PARAMETER_TYPES... info)->void{new_cpp_object = new T(std::forward<CONSTRUCTOR_PARAMETER_TYPES>(info)...);};

		// route any cpp exceptions through javascript
		try {
			CallCallable<decltype(constructor)>()(constructor,
												  info,
												  std::index_sequence_for<CONSTRUCTOR_PARAMETER_TYPES...>{},
												  DefaultArgsTupleType(*default_args_tuple_ptr));

		} catch(std::exception & e) {
			isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
			return;
		}


		// if the object was created by calling new in javascript, it should be deleted when the garbage collector
		//   GC's the javascript object, there should be no c++ references to it
		auto & deleter = *v8toolkit::V8ClassWrapper<T>::get_instance(isolate).destructor_behavior_delete;
		initialize_new_js_object(isolate, info.This(), new_cpp_object, deleter);

		// // return the object to the javascript caller
		info.GetReturnValue().Set(info.This());
	}
	
	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args);


public:

	// these probably shouldn't be public, but they are for now
	std::unique_ptr<DestructorBehavior> destructor_behavior_delete = std::make_unique<DestructorBehavior_Delete<T>>();
	std::unique_ptr<DestructorBehavior> destructor_behavior_leave_alone = std::make_unique<DestructorBehavior_LeaveAlone>();


	/**
     * Returns the embedded C++ object in a JavaScript object
     * @param object JavaScript object containing an embedded C++ object
     * @return The embedded C++ object
     */
	T * get_cpp_object(v8::Local<v8::Object> object);


	/**
     * Experimental
     * @param callback
     */
    void register_callback(PropertyChangedCallback callback);
    

	// Common tasks to do for any new js object regardless of how it is created
	static void initialize_new_js_object(v8::Isolate * isolate,
										 v8::Local<v8::Object> js_object,
										 T * cpp_object,
										 DestructorBehavior const & destructor_behavior)
	{
		auto any_ptr = new AnyPtr<T>(cpp_object);
#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "Initializing new js object for %s for v8::object at %p and cpp object at %p and any_ptr at %p\n", demangle<T>().c_str(), *js_object, cpp_object, (void*)any_ptr);
#endif
	    WrappedData<T> * wrapped_data = new WrappedData<T>(any_ptr);

#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "inserting anyptr<%s>at address %p pointing to cpp object at %p\n", typeid(T).name(), wrapped_data->native_object, cpp_object);
#endif
        if (js_object->InternalFieldCount() == 0) {
            fprintf(stderr, "Maybe you are calling a constructor without 'new'?");
        }
		assert(js_object->InternalFieldCount() >= 1);
	    js_object->SetInternalField(0, v8::External::New(isolate, wrapped_data));

		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));

		v8toolkit::global_set_weak(isolate, js_object, [isolate, cpp_object, &destructor_behavior]() {
			destructor_behavior(isolate, cpp_object);
		});
    }
	
	
    /**
    * Creates a new v8::FunctionTemplate capabale of creating wrapped T objects based on previously added methods and members.
    * TODO: This needs to track all FunctionTemplates ever created so it can try to use them in GetInstanceByPrototypeChain
    */
    v8::Local<v8::FunctionTemplate> make_wrapping_function_template(v8::FunctionCallback callback = nullptr,
								    const v8::Local<v8::Value> & data = v8::Local<v8::Value>());



    /**
    * Returns an existing constructor function template for the class/isolate OR creates one if none exist.
    *   This is to keep the number of constructor function templates as small as possible because looking up
    *   which one created an object takes linear time based on the number that exist
    */
    v8::Local<v8::FunctionTemplate> get_function_template();


	
	/**
	 * Check to see if an object can be converted to type T, else return nullptr
	 */
    T * cast(AnyBase * any_base);

    
    void init_instance_object_template(v8::Local<v8::ObjectTemplate> object_template);

    void init_prototype_object_template(v8::Local<v8::ObjectTemplate> object_template);

    void init_static_methods(v8::Local<v8::FunctionTemplate> constructor_function_template);



	
	/**
	* Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
	* For each isolate you need to add constructors/methods/members separately.
	*/
    static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate) {

		auto wrapper_find_result = isolate_to_wrapper_map.find(isolate);
        if ( wrapper_find_result != isolate_to_wrapper_map.end()) {
			return *wrapper_find_result->second;
		}
		return *new V8ClassWrapper<T>(isolate);
    }


    /**
     * Specify the name of the object which will be used for debugging statements as well as 
     *   being the type returned from javascript typeof
     */
    void set_class_name(const std::string & name);

    

    /**
    * Species other types that can be substituted for T when calling a function expecting T
    *   but T is not being passsed.   Only available for classes derived from T.
    * Every type is always compatible with itself and should not be specified here.
    * Not calling this means that only T objects will be accepted for things that want a T.
    * There is no automatic determination of inherited types by this library because I cannot
    *   figure out how.
    * It's VERY important that any type marked as having this type as a parent be marked as
    *   being a compatible type.
    */
    template<class... CompatibleTypes>
    std::enable_if_t<static_all_of<std::is_base_of<T,CompatibleTypes>::value...>::value>
    set_compatible_types()
    {
        assert(!is_finalized());

        if (!std::is_const<T>::value) {
            using ConstT = std::add_const_t<T>;
            V8ClassWrapper<ConstT>::get_instance(isolate).template set_compatible_types<std::add_const_t<CompatibleTypes>...>();
        }

        // Try to convert to T any of:  T, non-const T, any explicit compatible types and their const versions
		if (type_checker != nullptr) {
			// smart pointers are too compile-time expensive
			delete type_checker;
		}
        // TODO: EXPENSIVE
        type_checker = new TypeChecker<T, TypeList<std::add_const_t<T>, std::remove_const_t<T>, CompatibleTypes..., std::add_const_t<CompatibleTypes>...>>(this->isolate);
		if (this->wrap_as_most_derived_object != nullptr) {
			delete this->wrap_as_most_derived_object;
		}
		// TODO: EXPENSIVE
		this->wrap_as_most_derived_object = new WrapAsMostDerived<T, TypeList<CompatibleTypes...>>(this->isolate);
    }

	template<template<class> class Deleter>
	void set_deleter() {
		assert(!this->finalized);
		using ConstT = std::add_const_t<T>;
		if (!std::is_const<T>::value) {
			V8ClassWrapper<ConstT>::get_instance(this->isolate).template set_deleter<Deleter>();
		}
		this->destructor_behavior_delete = std::make_unique<Deleter<T>>();
	}
	
	
    /**
     * This wrapped class will inherit all the methods from the parent type (and its parent...)
     *
     * It is VERY important that the type being marked as the parent type has this type set with
     *   set_compatible_types<>()
     */
    template<class ParentType>
    std::enable_if_t<std::is_base_of<ParentType, T>::value>
    set_parent_type() {
		assert(!is_finalized());
		if (!std::is_const<T>::value) {
			using ConstT = std::add_const_t<T>;
			using ConstParent = std::add_const_t<ParentType>;
			V8ClassWrapper<ConstT>::get_instance(isolate).template set_parent_type<ConstParent>();
		}



		if (!V8ClassWrapper<ParentType>::get_instance(isolate).is_finalized()) {
			fprintf(stderr, "Tried to set parent type of %s to unfinalized %s\n",
					demangle<T>().c_str(), demangle<ParentType>().c_str());

		}
		assert(V8ClassWrapper<ParentType>::get_instance(isolate).is_finalized());
//	fprintf(stderr, "Setting parent of %s to %s\n", demangle<T>().c_str(), demangle<ParentType>().c_str());
		ISOLATE_SCOPED_RUN(isolate);
		global_parent_function_template =
			v8::Global<v8::FunctionTemplate>(isolate,
											 V8ClassWrapper<ParentType>::get_instance(isolate).get_function_template());

	}
    
    
	/**
	* V8ClassWrapper objects shouldn't be deleted during the normal flow of your program unless the associated isolate
	*   is going away forever.   Things will break otherwise as no additional objects will be able to be created
	*   even though V8 will still present the ability to your javascript (I think)
	*/
    virtual ~V8ClassWrapper();

	/**
	* Creates a javascript method with the specified name inside `parent_template` which, when called with the "new" keyword, will return
	*   a new object of this type.
	*/
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES, class DefaultArgsTuple = std::tuple<>>
	void add_constructor(const std::string & js_constructor_name,
						 v8::Local<v8::ObjectTemplate> parent_template,
						 DefaultArgsTuple const & default_args = DefaultArgsTuple())
	{
	    assert(((void)"Type must be finalized before calling add_constructor", this->finalized) == true);
		check_if_constructor_name_used(js_constructor_name);

	    auto constructor_template =
		make_wrapping_function_template(&V8ClassWrapper<T>::template v8_constructor<DefaultArgsTuple, CONSTRUCTOR_PARAMETER_TYPES...>,
						v8::External::New(this->isolate, new DefaultArgsTuple(std::move(default_args))));

	    // Add the constructor function to the parent object template (often the global template)
//	    std::cerr << "Adding constructor to global with name: " << js_constructor_name << std::endl;
	    parent_template->Set(v8::String::NewFromUtf8(isolate, js_constructor_name.c_str()), constructor_template);
	}


	/**
	 * When you don't want a "constructor" but you still need something to attach the static method names to, use this
	 * instead of add_constructor
	 */
    void expose_static_methods(const std::string & js_name,
                          v8::Local<v8::ObjectTemplate> parent_template) {
	    assert(((void)"Type must be finalized before calling expose_static_methods", this->finalized) == true);
		check_if_constructor_name_used(js_name);

        if (global_name_conflicts(js_name)) {
            throw V8Exception(this->isolate, "name conflicts with global names (bug: this ignores if your parent template isn't the global object)");
        }

	    auto non_constructor_template =
		make_wrapping_function_template([](const v8::FunctionCallbackInfo<v8::Value>& args)->void{
			throw V8Exception(args.GetIsolate(), "You cannot create an object of this type");
		    },
		    v8::Local<v8::Value>());

	    // Add the constructor function to the parent object template (often the global template)
//	    std::cerr << "Adding static-method holder (non-constructor) to global with name: " << js_name << std::endl;
	    parent_template->Set(v8::String::NewFromUtf8(isolate, js_name.c_str()), non_constructor_template);
	}


	/**
	* Used when wanting to return an object from a c++ function call back to javascript, or in conjunction with
    *   add_variable to give a javascript name to an existing c++ object 
    * \code{cpp}
    * add_variable(context, context->GetGlobal(), "js_name", class_wrapper.wrap_existing_cpp_object(context, some_c++_object));
    * \endcode
	*/
	v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object, DestructorBehavior const & destructor_behavior, bool force_wrap_this_type = false)
	{
		// TODO: Expensive - when combined with add_method -- maybe try and move this out of V8ClassWrapper?
		auto isolate = this->isolate;

        assert(existing_cpp_object != nullptr);

        // if it's not finalized, try to find an existing CastToJS conversion because it's not a wrapped class
	    //*** IF YOU ARE HERE LOOKING AT AN INFINITE RECURSION CHECK THE TYPE IS ACTUALLY WRAPPED ***
	    if (!this->is_finalized()) {
            // fprintf(stderr, "wrap existing cpp object cast to js %s\n", typeid(T).name());
            return CastToJS<T>()(isolate, *existing_cpp_object).template As<v8::Object>();
        }

#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p type: %s\n", existing_cpp_object, this, isolate, v8toolkit::demangle<T>().c_str());
#endif
		
		// if there's currently a javascript object wrapping this pointer, return that instead of making a new one
        //   This makes sure if the same object is returned multiple times, the javascript object is also the same
		v8::Local<v8::Object> javascript_object;
		if(this->existing_wrapped_objects.find(existing_cpp_object) != this->existing_wrapped_objects.end()) {
#ifdef V8_CLASS_WRAPPER_DEBUG
            fprintf(stderr, "Found existing javascript object for c++ object %p - %s\n", existing_cpp_object, v8toolkit::demangle<T>().c_str());
#endif
			javascript_object = v8::Local<v8::Object>::New(isolate, this->existing_wrapped_objects[existing_cpp_object]);

		} else {

            V8TOOLKIT_DEBUG("Creating new javascript object for c++ object %p - %s\n", existing_cpp_object, v8toolkit::demangle<T>().c_str());

			v8::Isolate::Scope is(isolate);
			v8::Context::Scope cs(context);

#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
            // try to get the javascript object inside a JSWrapper birectional object if it is one
			auto jswrapper_javascript_object = safe_get_javascript_object(existing_cpp_object);
			if (!jswrapper_javascript_object.IsEmpty()) {
				return jswrapper_javascript_object;
			}
#endif


			if (this->wrap_as_most_derived_flag && !force_wrap_this_type) {
                javascript_object = this->wrap_as_most_derived(existing_cpp_object);
            } else {
                javascript_object = get_function_template()->GetFunction()->NewInstance();

                // fprintf(stderr, "New object is empty?  %s\n", javascript_object.IsEmpty()?"yes":"no");
                // fprintf(stderr, "Created new JS object to wrap existing C++ object.  Internal field count: %d\n", javascript_object->InternalFieldCount());

                initialize_new_js_object(isolate, javascript_object, existing_cpp_object, destructor_behavior);

                this->existing_wrapped_objects.emplace(existing_cpp_object,
                                                       v8::Global<v8::Object>(isolate, javascript_object));

                V8TOOLKIT_DEBUG("Inserting new %s object into existing_wrapped_objects hash that is now of size: %d\n", typeid(T).name(), (int)this->existing_wrapped_objects.size());
                V8TOOLKIT_DEBUG("Wrap existing cpp object returning object about to be cast to a value: %s - %s\n", *v8::String::Utf8Value(javascript_object), v8toolkit::demangle<T>().c_str());
            }
		}
		return javascript_object;
	}



	
	template<class R, class... Params, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name, R(*callable)(Params...), DefaultArgs const & default_args_tuple = DefaultArgs{}) {
        static std::vector<std::string> reserved_names = {"arguments", "arity", "caller", "displayName",
                                                          "length", "name", "prototype"};

        if (std::find(reserved_names.begin(), reserved_names.end(), method_name) != reserved_names.end()) {
            throw InvalidCallException(fmt::format("The name: '{}' is a reserved property in javascript functions, so it cannot be used as a static method name", method_name));
        }

		if (!std::is_const<T>::value) {
			V8ClassWrapper<ConstT>::get_instance(isolate).add_static_method(method_name, callable);
		}

		// must be set before finalization
		assert(!this->finalized);

		this->check_if_static_name_used(method_name);


		auto static_method_adder = [this, method_name, callable](v8::Local<v8::FunctionTemplate> constructor_function_template) {

		    auto static_method_function_template = v8toolkit::make_function_template(this->isolate,
											     callable, method_name);
//		    fprintf(stderr, "Adding static method %s onto %p for %s\n", method_name.c_str(), &constructor_function_template, this->class_name.c_str());
		    constructor_function_template->Set(this->isolate,
						       method_name.c_str(),
						       static_method_function_template);
		};

		this->static_method_adders.emplace_back(static_method_adder);
	};



	template<class Callable, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name, Callable callable, DefaultArgs const & default_args_tuple = DefaultArgs{}) {
		if (!std::is_const<T>::value) {
			V8ClassWrapper<ConstT>::get_instance(isolate).add_static_method(method_name, callable);
		}

		// must be set before finalization
		assert(!this->finalized);

		this->check_if_static_name_used(method_name);

		auto static_method_adder = [this, method_name, callable](v8::Local<v8::FunctionTemplate> constructor_function_template) {

		    auto static_method_function_template = v8toolkit::make_function_template(this->isolate,
											     callable, method_name);

//		    fprintf(stderr, "Adding static method %s onto %p for %s\n", method_name.c_str(), &constructor_function_template, this->class_name.c_str());
		    constructor_function_template->Set(this->isolate,
						       method_name.c_str(),
						       static_method_function_template);
		};

		this->static_method_adders.emplace_back(static_method_adder);
	}

	    


    
    /**
    * Function to force API user to declare that all members/methods have been added before any
    *   objects of the wrapped type can be created to make sure everything stays consistent
    * Must be called before adding any constructors or using wrap_existing_object()
    */
	void finalize(bool wrap_as_most_derived = false);

    /**
    * returns whether finalize() has been called on this type for this isolate
    */
	bool is_finalized() {
        return this->finalized;
    }

    /**
    * Adds a getter and setter method for the specified class member
    * add_member(&ClassName::member_name, "javascript_attribute_name");
    */
    template<class MemberType,
		class MemberClass,
		MemberType (MemberClass::*member),
		std::enable_if_t<std::is_base_of<MemberClass, T>::value, int> = 0>
	void add_member(std::string const & member_name)
	{
	    assert(this->finalized == false);

	    if (!std::is_const<T>::value) {
			V8ClassWrapper<ConstT>::get_instance(isolate).
				template add_member_readonly<std::add_const_t<MemberType>, MemberClass, member>(member_name);
	    }

	    this->check_if_name_used(member_name);

	    // store a function for adding the member on to an object template in the future
	    member_adders.emplace_back([this, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){


		    constructor_template->SetAccessor(v8::Local<v8::Name>::Cast(v8::String::NewFromUtf8(isolate, member_name.c_str())),
						      _getter_helper<MemberType, MemberClass, member>,
						      _setter_helper<MemberType, MemberClass, member>);
		});
	}


    template<class MemberType,
		class MemberClass, 	// allow members from parent types of T
		MemberType (MemberClass::*member),
		std::enable_if_t<std::is_base_of<MemberClass, T>::value, int> = 0>
	void add_member_readonly(std::string const & member_name)
	{
	    // make sure to be using the const version even if it's not passed in
	    using ConstMemberType = std::add_const_t<MemberType>;

	    // the field may be added read-only even to a non-const type, so make sure it's added to the const type, too
	    if (!std::is_const<T>::value) {
		    V8ClassWrapper<ConstT>::get_instance(isolate).template add_member_readonly<ConstMemberType, MemberClass, member>(member_name);
	    }

	    assert(this->finalized == false);

	    this->check_if_name_used(member_name);

	    member_adders.emplace_back([this, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){

		    constructor_template->SetAccessor(v8::String::NewFromUtf8(isolate, member_name.c_str()),
						      _getter_helper<MemberType, MemberClass, member>,
						      0);
		});
	}


	/**
	 * The specified function will be called when the JavaScript object is called like a function
	 * @param method function to call
	 */
	template<class R, class TBase, class... Args,
			 std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
	void make_callable(R(TBase::*method)(Args...))
	{
	    _add_method("unused name", method, TypeList<Args...>(), std::tuple<>(), true);
	}



	/**
	 * Adds const-qualified member instance functions
	 * @param method_name JavaScript property name to use
	 * @param method member instance function pointer
	 * @param default_args any default arguments to be used if insufficient JavaScript arguments provided
	 */
	template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
            std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
      void add_method(const std::string & method_name, R(TBase::*method)(Args...) const, DefaultArgs const & default_args = DefaultArgs()) {
        if (!std::is_const<T>::value) {
            V8ClassWrapper<std::add_const_t<T>>::get_instance(isolate)._add_method(method_name, method, TypeList<Args...>(), default_args);
        }
        _add_method(method_name, method, TypeList<Args...>(), default_args);
    }

	/**
	 * Adds const-and-lvalue-qualified member instance functions
	 * @param method_name JavaScript property name to use
	 * @param method member instance function pointer
	 * @param default_args any default arguments to be used if insufficient JavaScript arguments provided
	 */
    template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
            std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
      void add_method(const std::string & method_name, R(TBase::*method)(Args...) const &, DefaultArgs const & default_args = DefaultArgs()) {
     if (!std::is_const<T>::value) {
            V8ClassWrapper<std::add_const_t<T>>::get_instance(isolate)._add_method(method_name, method, TypeList<Args...>(), default_args);
        }
        _add_method(method_name, method, TypeList<Args...>(), default_args);
    }


	/**
	 * const rvalue instance functions not supported yet.  Using this triggers a static assertion failure
	 * @param method_name ignored
	 * @param method member ignored
	 * @param default_args ignored
	 */
	template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
            std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
      void add_method(const std::string & method_name, R(TBase::*method)(Args...) const &&, DefaultArgs const & default_args = DefaultArgs()) {
        static_assert(std::is_same<R, void>::value && !std::is_same<R, void>::value, "not supported");
    }


	/**
	 * Adds const-and-lvalue-qualified member instance functions
	 * @param method_name JavaScript property name to use
	 * @param method member instance function pointer
	 * @param default_args any default arguments to be used if insufficient JavaScript arguments provided
	 */
	template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
			 std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
	void add_method(const std::string & method_name, R(TBase::*method)(Args...), DefaultArgs const & default_args = DefaultArgs())
	{
		_add_method(method_name, method, TypeList<Args...>(), default_args);
	}

	/**
	 * Adds lvalue-qualified member instance functions
	 * @param method_name JavaScript property name to use
	 * @param method member instance function pointer
	 * @param default_args any default arguments to be used if insufficient JavaScript arguments provided
	 */
    template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
            std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
    void add_method(const std::string & method_name, R(TBase::*method)(Args...) &, DefaultArgs const & default_args = DefaultArgs())
    {
		_add_method(method_name, method, TypeList<Args...>(), default_args);
    }


	/**
	 * rvalue instance functions not supported yet.  Using this triggers a static assertion failure
	 * @param method_name ignored
	 * @param method member ignored
	 * @param default_args ignored
	 */
	template<class R, class TBase, class... Args, class DefaultArgs = std::tuple<>,
            std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
    void add_method(const std::string & method_name, R(TBase::*method)(Args...) &&, DefaultArgs const & default_args = DefaultArgs())
    {
        static_assert(std::is_same<R, void>::value && !std::is_same<R, void>::value, "not supported");
    }




    /**
	* If the method is marked const, add it to the const version of the wrapped type
	*/
	template<class R, class Head, class... Tail, class DefaultArgs = std::tuple<>,
        std::enable_if_t<std::is_const<Head>::value && !std::is_const<T>::value, int> = 0>
	void add_fake_method_for_const_type(const std::string & method_name, func::function<R(Head *, Tail...)> method,
                                        DefaultArgs const & default_args = DefaultArgs()) {
		V8ClassWrapper<ConstT>::get_instance(isolate)._add_fake_method(method_name, method, default_args);
	};


	/**
	 * If the method is not marked const, don't add it to the const type (since it's incompatible)
	 */
	template<class R, class Head, class... Tail, class DefaultArgs = std::tuple<>,
        std::enable_if_t<!(std::is_const<Head>::value && !std::is_const<T>::value), int> = 0>
	void add_fake_method_for_const_type(const std::string & method_name, func::function<R(Head *, Tail...)> method,
                                        DefaultArgs const & default_args = DefaultArgs()) {
		// nothing to do here
	};


	/**
	 * Creates a "fake" method from any callable which takes a T* as its first parameter.  Does not create
	 *   the method on the const type
	 * @param method_name JavaScript name to expose this method as
	 * @param method method to call when JavaScript function invoked
	 */
    template<class R, class Head, class... Args, class DefaultArgs = std::tuple<>,
	std::enable_if_t<std::is_pointer<Head>::value && // Head must be T * or T const *
					 std::is_same<std::remove_const_t<std::remove_pointer_t<Head>>, T>::value, int> = 0>
	void add_method(const std::string & method_name, func::function<R(Head, Args...)> & method,
                    DefaultArgs const & default_args = DefaultArgs()) {
		_add_fake_method(method_name, method, default_args);
	}


	/**
	 * Creates a "fake" method from any callable which takes a T* as its first parameter.  Does not create
	 *   the method on the const type
	 * @param method_name JavaScript name to expose this method as
	 * @param method method to call when JavaScript function invoked
	 */
	template<class R, class Head, class... Args, class DefaultArgs = std::tuple<>,
		std::enable_if_t<std::is_pointer<Head>::value &&
						 std::is_base_of<std::remove_const_t<std::remove_pointer_t<Head>>, T>::value, int> = 0>
	void add_method(const std::string & method_name, R(*method)(Head, Args...),
                    DefaultArgs const & default_args = DefaultArgs()) {

		_add_fake_method(method_name, func::function<R(Head, Args...)>(method), default_args);
	}


	/**
	 * Takes a lambda taking a T* as its first parameter and creates a 'fake method' with it
	 */
	template<class Callback, class DefaultArgs = std::tuple<>   >
	void add_method(const std::string & method_name, Callback && callback,
                                   DefaultArgs const & default_args = DefaultArgs()) {
		decltype(LTG<Callback>::go(&Callback::operator())) f(callback);
		this->_add_fake_method(method_name, f, default_args);

	}


	v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) {
		return this->wrap_as_most_derived_object->operator()(cpp_object);
	}


	v8::Local<v8::Object> wrap_as_most_derived(T && cpp_object) {
		return this->wrap_as_most_derived_object->operator()(std::move(cpp_object));
	}


	template<class R, class Head, class... Tail, class DefaultArgsTuple,
		std::enable_if_t<std::is_pointer<Head>::value && // Head must be T * or T const *
						 std::is_same<std::remove_const_t<std::remove_pointer_t<Head>>, std::remove_const_t<T>>::value, int> = 0>
	void _add_fake_method(const std::string & method_name, func::function<R(Head, Tail...)> method, DefaultArgsTuple const & default_args)
	{
		assert(this->finalized == false);

		// conditionally add the method to the const type
		add_fake_method_for_const_type(method_name, method);

		this->check_if_name_used(method_name);


		// This puts a function on a list that creates a new v8::FunctionTemplate and maps it to "method_name" on the
		// Object template that will be passed in later when the list is traversed
		fake_method_adders.emplace_back([this, default_args, method_name, method](v8::Local<v8::ObjectTemplate> prototype_template) {

			using CopyFunctionType = func::function<R(Head, Tail...)>;
			CopyFunctionType * copy = new func::function<R(Head, Tail...)>(method);


			// This is the actual code associated with "method_name" and called when javascript calls the method
			StdFunctionCallbackType * method_caller =
					new StdFunctionCallbackType([method_name, default_args, copy](const v8::FunctionCallbackInfo<v8::Value>& info) {


				auto fake_method = *(func::function<R(Head, Tail...)>*)v8::External::Cast(*(info.Data()))->Value();
				auto isolate = info.GetIsolate();

				auto holder = info.Holder();

                // it should not be 0, and right now there is no known way for it being more than 1.
                assert(holder->InternalFieldCount() == 1);

                // a crash here may have something to do with a native override of toString
				auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());


				// V8 does not support C++ exceptions, so all exceptions must be caught before control
				//   is returned to V8 or the program will instantly terminate
				try {
                    CallCallable<CopyFunctionType, Head>()(*copy, info, cpp_object, std::index_sequence_for<Tail...>(), default_args); // just Tail..., not Head, Tail...
				} catch(std::exception & e) {
					isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
					return;
				}
				return;
			});

			// create a function template, set the lambda created above to be the handler
			auto function_template = v8::FunctionTemplate::New(this->isolate);
			function_template->SetCallHandler(callback_helper, v8::External::New(this->isolate, method_caller));

			// methods are put into the protype of the newly created javascript object
			prototype_template->Set(v8::String::NewFromUtf8(isolate, method_name.c_str()), function_template);
		});
	}

	/**
	 * A list of methods to be added to each object
	 */




    template<class M, class... Args, class... DefaultArgTypes>
	void _add_method(const std::string & method_name,
                                    M method,
                                    TypeList<Args...> const &,
                                    std::tuple<DefaultArgTypes...> const & default_args_tuple,
                                    bool add_as_callable_object_callback = false)
    {
        // TODO: EXPENSIVE - when combined with
        assert(this->finalized == false);

		this->check_if_name_used(method_name);

    		MethodAdderData method_adder_data = MethodAdderData{method_name,
                                                 StdFunctionCallbackType([this, default_args_tuple, method, method_name](const v8::FunctionCallbackInfo<v8::Value>& info) {
                auto isolate = info.GetIsolate();

                // get the behind-the-scenes c++ object
                // However, Holder() refers to the most-derived object, so the prototype chain must be
                //   inspected to find the appropriate v8::Object with the T* in its internal field
                auto holder = info.Holder();
                v8::Local<v8::Object> self;

#ifdef V8_CLASS_WRAPPER_DEBUG
                    fprintf(stderr, "Looking for instance match in prototype chain %s :: %s\n", demangle<T>().c_str(),
                            demangle<M>().c_str());
                fprintf(stderr, "Holder: %s\n", stringify_value(isolate, holder).c_str());
                dump_prototypes(isolate, holder);
#endif

                auto function_template_count = this->this_class_function_templates.size();
                int current_template_count = 0;
                for (auto &function_template : this->this_class_function_templates) {
                    current_template_count++;
                    V8TOOLKIT_DEBUG("Checking function template %d / %d\n", current_template_count,
                                (int) function_template_count);
                    self = holder->FindInstanceInPrototypeChain(function_template.Get(isolate));
                    if (!self.IsEmpty() && !self->IsNull()) {
                        V8TOOLKIT_DEBUG("Found instance match in prototype chain\n");
                        break;
                    } else {
                        V8TOOLKIT_DEBUG("No match on this one\n");
                    }
                }
                if (self.IsEmpty()) {
                    V8TOOLKIT_DEBUG("No match in prototype chain after looking through all potential function templates\n");
                    assert(false);
                }


                // void* pointer = instance->GetAlignedPointerFromInternalField(0);
                auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));

//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "uncasted internal field: %p\n", wrap->Value());
                WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
                auto backing_object_pointer = V8ClassWrapper<T>::get_instance(isolate).cast(
                    static_cast<AnyBase *>(wrapped_data->native_object));

//			    assert(backing_object_pointer != nullptr);
                // bind the object and method into a func::function then build the parameters for it and call it
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "binding with object %p\n", backing_object_pointer);
                auto bound_method = v8toolkit::bind<T>(*backing_object_pointer, method);



                // V8 does not support C++ exceptions, so all exceptions must be caught before control
                //   is returned to V8 or the program will instantly terminate
                try {
					// make a copy of default_args_tuple so it's non-const - probably better to do this on a per-parameter basis
                    CallCallable<decltype(bound_method)>()(bound_method, info, std::index_sequence_for<Args...>{}, std::tuple<DefaultArgTypes...>(default_args_tuple));
                } catch (std::exception &e) {
                    isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
                    return;
                }
                return;
            })};

		if (add_as_callable_object_callback) {
		    // can only set this once
		    assert(!callable_adder.callback);
		    callable_adder = method_adder_data;
		} else {
		    method_adders.push_back(method_adder_data);
		}
	}

	/**
	 * http://v8.paulfryzel.com/docs/master/classv8_1_1_object_template.html#ae3303f3d55370684ac02b02e67712eac
	 * This version hasn't been enhanced yet like the named property version has.
	 * Sets a callback when some_object[4] is called
	 * @param callable function to be called
	 */
	void add_index_getter(v8::IndexedPropertyGetterCallback getter) {
		// can only set one per class type
		assert(indexed_property_getter == nullptr);
		indexed_property_getter = getter;
	}


	struct NamedPropertyCallbackData {
		T * cpp_object = nullptr;
        func::function<void(v8::Local<v8::Name> property_name,
                           v8::PropertyCallbackInfo<v8::Value> const &)> getter;
        func::function<void(v8::Local<v8::Name> property_name,
                           v8::Local<v8::Value> new_property_value,
                           v8::PropertyCallbackInfo<v8::Value> const &)> setter;
	};

	template<class ReturnT>
	using NamedPropertyGetter = func::function<ReturnT(T*, std::string const &)>;

    template<class ReturnT>
    using NamedPropertySetter = func::function<ReturnT(T*, std::string const &)>;




	class NoResultAvailable : public std::exception {
	public:
		virtual const char * what() const noexcept override {return "";}
	};


	/* Helper for handling pointer return types from named property getter
	 * allows checking for null pointers
	 */
	template<class ResultT>
	std::enable_if_t<std::is_pointer<ResultT>::value, void> handle_getter_result(ResultT&& result,
																		   v8::PropertyCallbackInfo<v8::Value> const &info) {
		if (result != nullptr) {
			info.GetReturnValue().Set(CastToJS<ResultT>()(info.GetIsolate(), std::forward<ResultT>(result)));
		}
	}

	/* Helper for handling non-pointer return types from named property getter */
	template<class ResultT>
	std::enable_if_t<!std::is_pointer<ResultT>::value, void> handle_getter_result(ResultT&& result,
																		   v8::PropertyCallbackInfo<v8::Value> const &info) {

		info.GetReturnValue().Set(CastToJS<ResultT>()(info.GetIsolate(), std::move(result)));

	}


	/**
	 * http://v8.paulfryzel.com/docs/master/classv8_1_1_object_template.html#a66fa7b04c87676e20e35497ea09a0ad0
	 * Returning either a nullptr or throwing NoResultAvailable exception means the value was not found
	 * @param callback function to be called
	 */
	template<class GetterReturnT, class SetterReturnT>
	void add_named_property_handler(NamedPropertyGetter<GetterReturnT> getter_callback,
                                    NamedPropertySetter<SetterReturnT> setter_callback) {
		assert(!this->finalized);
		assert(!this->named_property_adder);
		named_property_adder = [this, getter_callback, setter_callback](v8::Local<v8::ObjectTemplate> object_template) {

			auto data = new NamedPropertyCallbackData();

			if (getter_callback) {
				data->getter = [this, getter_callback](v8::Local<v8::Name> property_name,
													   v8::PropertyCallbackInfo<v8::Value> const &info) {
					// don't know how to handle symbols - can't find a way to stringify them
					if (property_name->IsSymbol()) {
						printf("symbol name: %s\n", *v8::String::Utf8Value(v8::Local<v8::Symbol>::Cast(property_name)->Name()));
						return;
					}
					T *cpp_object = v8toolkit::V8ClassWrapper<T>::get_cpp_object(info.This());

					try {
						auto str = *v8::String::Utf8Value(property_name);
						assert(str);
							handle_getter_result(getter_callback(cpp_object, str), info);
						return;
					} catch(NoResultAvailable) {
						return;
					}
				};
			}
			if (setter_callback) {
				data->setter = [this, setter_callback](v8::Local<v8::Name> property_name,
													   v8::Local<v8::Value> new_property_value,
													   v8::PropertyCallbackInfo<v8::Value> const &info) {

					T *cpp_object = v8toolkit::V8ClassWrapper<T>::get_cpp_object(info.This());
					setter_callback(cpp_object, *v8::String::Utf8Value(property_name)) =
							CastToNative<std::remove_reference_t<typename ProxyType<SetterReturnT>::PROXY_TYPE>>()(isolate,
																						  new_property_value);
					info.GetReturnValue().Set(true);
				};
			}


			object_template->SetHandler(v8::NamedPropertyHandlerConfiguration(
					// Getter
                    [](v8::Local<v8::Name> property_name,
					   v8::PropertyCallbackInfo<v8::Value> const & info){
					    auto external_data = v8::External::Cast(*info.Data());
					    NamedPropertyCallbackData * data = static_cast<NamedPropertyCallbackData *>(external_data->Value());
					    data->getter(property_name, info);
				    },
                    // setter
                    [](v8::Local<v8::Name> property_name,
                       v8::Local<v8::Value> new_property_value,
                       v8::PropertyCallbackInfo<v8::Value> const & info){
                        auto external_data = v8::External::Cast(*info.Data());
                        NamedPropertyCallbackData * data = static_cast<NamedPropertyCallbackData *>(external_data->Value());
                        data->setter(property_name, new_property_value, info);
                    },
					// query - returns attributes on the given property name
					// http://brendanashworth.github.io/v8-docs/namespacev8.html#a05f25f935e108a1ea2d150e274602b87
					[](v8::Local< v8::Name > property_name, v8::PropertyCallbackInfo< v8::Integer> const & info){
						printf("In query callback %s\n", *v8::String::Utf8Value(property_name));
					},
					// deleter
					[](v8::Local<v8::Name> property_name,
					   v8::PropertyCallbackInfo<v8::Boolean> const & info){
						printf("IN DELETER CALLBACK %s\n", *v8::String::Utf8Value(property_name));
					},
					// enumerator
					[](v8::PropertyCallbackInfo<v8::Array> const & info) {
						printf("IN ENUMERATOR CALLBACK\n");
						info.GetReturnValue().Set(v8::Array::New(info.GetIsolate()));
					},
					v8::External::New(this->isolate, (void *)data),
					v8::PropertyHandlerFlags::kNonMasking // don't call on properties that exist
//					v8::PropertyHandlerFlags::kNone // call on everything (doesn't work well with added methods/members)
			));
		};
	}


	/**
	 * This version accepts a &T::operator[] directly -- does the std::bind for you
	 * @param callback T instance member function to be called
	 */
	template<class GetterReturnT, class GetterObjectT, class SetterReturnT, class SetterObjectT>
	void add_named_property_handler(GetterReturnT(GetterObjectT::*getter_callback)(std::string const &) const,
                                    SetterReturnT(SetterObjectT::*setter_callback)(std::string const &)) {
		NamedPropertyGetter<GetterReturnT> bound_getter;
		NamedPropertySetter<SetterReturnT> bound_setter;
		if (getter_callback != nullptr) {
            bound_getter = std::bind(getter_callback, std::placeholders::_1, std::placeholders::_2);
        }
        if (setter_callback != nullptr) {
            bound_setter = std::bind(setter_callback, std::placeholders::_1, std::placeholders::_2);
        }

		this->add_named_property_handler(bound_getter, bound_setter);
	}



	/**
	 * ADVANCED: allows direct customization of the v8::FunctionTemplate used to create objects
	 * Use for anything this library doesn't take care of
	 */
	void add_new_constructor_function_template_callback(FunctionTemplateCallback const & callback) {
		assert(!this->finalized);
		function_template_callbacks.push_back(callback);
	}

};

} // end v8toolkit namespace

#ifndef V8TOOLKIT_WRAPPER_FAST_COMPILE
#include "v8_class_wrapper_impl.h"
#endif

namespace v8toolkit {
 
/**
* Stores the "singleton" per isolate
*/
template <class T>
    MapT<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::isolate_to_wrapper_map;

template<class T>
class JSWrapper;


template<typename T, class>
struct CastToJS {

    
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object){
	    V8TOOLKIT_DEBUG("CastToJS from lvalue ref %s\n", demangle<T>().c_str());
		return CastToJS<typename std::add_pointer<T>::type>()(isolate, &cpp_object);
	}

	/**
	* If an rvalue is passed in, a copy must be made.
	*/
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object){
        using NoRefT = std::remove_reference_t<T>;
		V8TOOLKIT_DEBUG("In base cast to js struct with rvalue ref");
		V8TOOLKIT_DEBUG("Asked to convert rvalue type, so copying it first\n");

		// this memory will be owned by the javascript object and cleaned up if/when the GC removes the object
		auto copy = new NoRefT(std::move(cpp_object));
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<NoRefT> & class_wrapper = V8ClassWrapper<NoRefT>::get_instance(isolate);
		auto result = class_wrapper.template wrap_existing_cpp_object(context, copy, *class_wrapper.destructor_behavior_delete);
        V8TOOLKIT_DEBUG("CastToJS<T> returning wrapped existing object: %s\n", *v8::String::Utf8Value(result));
        
        return result;
	}
};



 
/**
* Attempt to use V8ClassWrapper to wrap any remaining types not handled by the specializations in casts.hpp
* That type must have had its methods and members added beforehand in the same isolate
*/
 template<typename T>
struct CastToJS<T*, std::enable_if_t<std::is_polymorphic<T>::value>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){
	    if (cpp_object == nullptr) {
		    return v8::Undefined(isolate);
	    }

	    assert(cpp_object != (void *)0xbebebebebebebebe);
	    
		V8TOOLKIT_DEBUG("CastToJS from T* %s\n", demangle_typeid_name(typeid(T).name()).c_str());
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
		// if the type is polymorphic and potentially bidirectional, check to see if it actually is
		using JSWrapperType = JSWrapper<std::remove_const_t<T>>;
//		fprintf(stderr, "Checking to see if object * is a JSWrapper *\n");

		if (std::is_const<T>::value) {
			auto js_wrapper = dynamic_cast<JSWrapperType const *>(cpp_object);
			if (js_wrapper) {
				return CastToJS<const JSWrapperType>()(isolate, *js_wrapper);
			}
		} else {
			// this only runs if it's non-const, so casting is not scary - only to trick compiler
			using NonConstT = std::remove_const_t<T>;
			auto js_wrapper = dynamic_cast<JSWrapperType *>(const_cast<NonConstT *>(cpp_object));
			if (js_wrapper) {
				return CastToJS<JSWrapperType>()(isolate, *js_wrapper);
			}
		}

#endif
		V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

		/** If you are here looking for an INFINITE RECURSION make sure the type is wrapped **/
		return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
	}
};

template<typename T>
struct CastToJS<T*, std::enable_if_t<!std::is_polymorphic<T>::value>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){

	    if (cpp_object == nullptr) {
		return v8::Local<v8::Object>();
	    }
	    assert(cpp_object != (void *)0xbebebebebebebebe);


	    V8TOOLKIT_DEBUG("CastToJS from T* %s\n", demangle<T>().c_str());
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

		V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

		return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
	}

 };

 template<typename T>
     struct CastToJS<T * const> {
     v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const cpp_object){

         if (cpp_object == nullptr) {
             return v8::Local<v8::Object>();
         }
         assert(cpp_object != (void *)0xbebebebebebebebe);

         V8TOOLKIT_DEBUG("CastToJS from T* {}\n", demangle<T>().c_str());
         auto context = isolate->GetCurrentContext();
         auto & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

         V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

         return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
     }
 };


    template<typename T>
    struct CastToJS<T const * const> {
        v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * cpp_object){

            if (cpp_object == nullptr) {
                return v8::Local<v8::Object>();
            }
            assert(cpp_object != (void *)0xbebebebebebebebe);

            V8TOOLKIT_DEBUG("CastToJS from T* {}\n", demangle<T>().c_str());
            auto context = isolate->GetCurrentContext();
            auto & class_wrapper = V8ClassWrapper<T const>::get_instance(isolate);

            V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

            return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
        }
    };




template<typename T>
struct CastToJS<T&> {
	using Pointer = typename std::add_pointer_t<T>;

	v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_const_t<T> & cpp_object){
		return CastToJS<Pointer>()(isolate, &cpp_object);
	}
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::add_const_t<T> & cpp_object){
        return CastToJS<Pointer>()(isolate, &cpp_object);
    }

};
template<typename T>
struct CastToJS<T*&> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){
		return CastToJS<T*>()(isolate, cpp_object);
	}
};

template<typename T>
struct CastToJS<T const *&> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * cpp_object){
		return CastToJS<T const *>()(isolate, cpp_object);
	}
};
template<typename T>
struct CastToJS<T* const &> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const cpp_object){
		return CastToJS<T * const>()(isolate, cpp_object);
	}
};
template<typename T>
struct CastToJS<T const * const &> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * const cpp_object){
		return CastToJS<T const * const>()(isolate, cpp_object);
	}
};


template<class T>
std::string type_details(){
    return fmt::format("const: {} pointer: {} reference: {} typeid: {}",
		       std::is_const<T>::value, std::is_pointer<T>::value,
		       std::is_reference<T>::value, typeid(T).name());
 }


// specialization for pointers and reference types
 template<class T, std::enable_if_t<std::is_pointer<T>::value || std::is_reference<T>::value, int> = 0>
	T & get_object_from_embedded_cpp_object(v8::Isolate * isolate, v8::Local<v8::Value> value) {
     throw CastException(fmt::format("Pointer and reference types ({}) won't ever succeed in getting an embedded cpp object", demangle<T>()));
 }

/**
 * This can be used from CastToNative<UserType> calls to fall back to if other conversions aren't appropriate
 */
template<class T, std::enable_if_t<!std::is_pointer<T>::value && !std::is_reference<T>::value, int> = 0>
T & get_object_from_embedded_cpp_object(v8::Isolate * isolate, v8::Local<v8::Value> value) {

	V8TOOLKIT_DEBUG("cast to native\n");
	if(!value->IsObject()){
		V8TOOLKIT_DEBUG("CastToNative failed for type: %s (%s)\n", type_details<T>().c_str(), *v8::String::Utf8Value(value));
		v8toolkit::print_v8_value_details(value);
		V8TOOLKIT_DEBUG("stringified value: %s\n", stringify_value(isolate, value).c_str());

		throw CastException(fmt::format("No specialized CastToNative found and value was not a Javascript Object: {}", demangle<T>()));
	}
	auto object = v8::Object::Cast(*value);
	if (object->InternalFieldCount() <= 0) {
		throw CastException(fmt::format("No specialization CastToNative<{}> found (for any shortcut notation) and provided Object is not a wrapped C++ object.  It is a native Javascript Object", demangle<T>()));
	}
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
	auto wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());

	auto any_base = (v8toolkit::AnyBase *)wrapped_data->native_object;
	T * t = nullptr;
//	 std::cerr << fmt::format("about to call cast on {}", demangle<T>()) << std::endl;
	if ((t = V8ClassWrapper<T>::get_instance(isolate).cast(any_base)) == nullptr) {
		fprintf(stderr, "Failed to convert types: want:  %d %s\n", std::is_const<T>::value, typeid(T).name());
		throw CastException(fmt::format("Cannot convert AnyBase to {}", demangle<T>()));
	}
//		std::cerr << fmt::format("Successfully converted") << std::endl;
	return *t;
}

// excluding types where CastToNative doesn't return a reference type
// this stops trying &int when int is an rvalue
//   when trying to deal with unique_ptr in casts.hpp (in an unused but still compiled code path)
template<typename T>
struct CastToNative<T*, std::enable_if_t<std::is_reference<
		std::result_of_t<
				CastToNative<std::remove_pointer_t<T>>(v8::Isolate*, v8::Local<v8::Value>)
						> // end result_of
		>::value // end is_reference
		>// end enable_if_t
		>// end template
{
	   T * operator()(v8::Isolate * isolate, v8::Local<v8::Value> value){
           if (value->IsUndefined()) {
               return nullptr;
           }
           return & CastToNative<typename std::remove_reference<T>::type>()(isolate, value);
		}
};

template<typename T, class>
struct CastToNative
{
	T & operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		return get_object_from_embedded_cpp_object<T>(isolate, value);
	}
};


template<typename T>
struct CastToNative<T&, std::enable_if_t<std::is_reference<
    std::result_of_t<
        CastToNative<T>(v8::Isolate*, v8::Local<v8::Value>)
    > // end result_ofs
>::value>>
{
    T & operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
        return get_object_from_embedded_cpp_object<T>(isolate, value);
    }
};

// cannot get a reference unless the object is stored inside a javascript object
template<typename T>
struct CastToNative<T&, std::enable_if_t<!std::is_reference<
    std::result_of_t<
        CastToNative<T>(v8::Isolate*, v8::Local<v8::Value>)
    > // end result_ofs
>::value>>;



template<typename T>
struct CastToNative<T&&, std::enable_if_t<std::is_reference<
    std::result_of_t<
        CastToNative<T>(v8::Isolate*, v8::Local<v8::Value>)
    > // end result_ofs
>::value>>
{
    T & operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
        return get_object_from_embedded_cpp_object<T>(isolate, value);
    }
};

// cannot get a reference unless the object is stored inside a javascript object
template<typename T>
struct CastToNative<T&&, std::enable_if_t<!std::is_reference<
    std::result_of_t<
        CastToNative<T>(v8::Isolate*, v8::Local<v8::Value>)
    > // end result_ofs
>::value>>;




// If no more-derived option was found, wrap as this type
template<class T>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<>>::operator()(T * cpp_object) const {
	auto context = this->isolate->GetCurrentContext();

		// TODO: Expensive
	auto & wrapper = v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	return wrapper.template wrap_existing_cpp_object(context, cpp_object, *wrapper.destructor_behavior_leave_alone, true /* don't infinitely recurse */);
}

template<class T>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<>>::operator()(T && cpp_object) const {
	auto context = this->isolate->GetCurrentContext();

	// TODO: expensive
	auto & wrapper = v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	return wrapper.
			template wrap_existing_cpp_object(context,
                                              safe_move_constructor(std::move(cpp_object)).
                                                  release(),
                                              *wrapper.destructor_behavior_delete, true /* don't infinitely recurse */);
    }



template<class T, class Head, class... Tail> T *
TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,
            std::enable_if_t<std::is_const<T>::value ||
					         !std::is_const<Head>::value>
           >::check(AnyBase * any_base, bool first_call) const {
	assert(any_base != nullptr);
	ANYBASE_PRINT("typechecker::check for {}  with anyptr {} (string: {})", demangle<Head>(), (void*)any_base, any_base->type_name);
	if(auto any = dynamic_cast<AnyPtr<Head> *>(any_base)) {
		ANYBASE_PRINT("Got match on: {}, returning {}", demangle<Head>(), (void*)(any->get()));
		return static_cast<T*>(any->get());
	}
	
	ANYBASE_PRINT("didn't find match, testing const type now...");

	// TODO: Expensive
	// if goal type is const and the type to check isn't const, try checking for the const type now
	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
        if (auto derived_result = V8ClassWrapper<Head>::get_instance(this->isolate).cast(any_base)) {
            return derived_result;
        }
	}
	
	ANYBASE_PRINT("no match on const type either, continuing down chain");

	return SUPER::check(any_base, false);
}




// if a more-derived type was found, pass it to that type to see if there's something even more derived
template<class T, class Head, class... Tail>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<Head, Tail...>,
	std::enable_if_t<!std::is_const<T>::value || std::is_const<Head>::value>>
::operator()(T * cpp_object) const {

	// if they're the same, let it fall through to the empty typechecker TypeList base case
	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
		using MatchingConstT = std::conditional_t<std::is_const<Head>::value, std::add_const_t<T>, std::remove_const_t<T>>;

		if (std::is_const<T>::value == std::is_const<Head>::value) {
			if (auto derived = safe_dynamic_cast<Head *>(const_cast<MatchingConstT *>(cpp_object))) {
				// TODO: Expensive
				return v8toolkit::V8ClassWrapper<Head>::get_instance(this->isolate).wrap_as_most_derived(derived);
			}
		}
	}
	return SUPER::operator()(cpp_object);
}

template<class T, class Head, class... Tail>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<Head, Tail...>,
		std::enable_if_t<!std::is_const<T>::value || std::is_const<Head>::value>>
::operator()(T && cpp_object) const {
	// if they're the same, let it fall through to the empty typechecker TypeList base case
	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
		using MatchingConstT = std::conditional_t<std::is_const<Head>::value, std::add_const_t<T>, std::remove_const_t<T>>;

		if (std::is_const<T>::value == std::is_const<Head>::value) {
			if (auto derived = safe_dynamic_cast<Head *>(const_cast<MatchingConstT *>(&cpp_object))) {
				// TODO: Expensive
				return v8toolkit::V8ClassWrapper<Head>::get_instance(this->isolate).wrap_as_most_derived(std::move(*derived));
			}
		}
	}
	return SUPER::operator()(std::move(cpp_object));

};




} // end namespace v8toolkit







































