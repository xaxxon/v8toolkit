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
#include "casts.h"

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

	// wehther this destructor is actually destructive to memory it is given.  Does it "own" the memory or not.
	virtual bool destructive() const = 0;

	virtual std::string name() const = 0;
};


/**
* Helper to delete a C++ object when the corresponding javascript object is garbage collected
*/
template<class T>
struct DestructorBehavior_Delete : DestructorBehavior {
	DestructorBehavior_Delete(){
//		std::cerr << fmt::format("creating destructor behavior delete at {}", (void*)this) << std::endl;
	}
	void operator()(v8::Isolate * isolate, const void * void_object) const override {
		T* object = (T*)void_object;
        V8TOOLKIT_DEBUG("Deleting underlying C++ object at %p during V8 garbage collection\n", void_object);
		delete object;
		isolate->AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(sizeof(T)));
	}
	bool destructive() const override {
//		std::cerr << fmt::format("_Delete::destructive") << std::endl;
		return true;
	}
	virtual std::string name() const override {
		return fmt::format("_delete");
	}

};

/**
* Helper to not do anything to the underlying C++ object when the corresponding javascript object
*   is garbage collected
*/
struct DestructorBehavior_LeaveAlone : DestructorBehavior {
	DestructorBehavior_LeaveAlone(){
//		std::cerr << fmt::format("creating DestructorBehavior_LeageAlone at {}", (void*)this) << std::endl;
	}

	void operator()(v8::Isolate * isolate, const void * void_object) const override {
        V8TOOLKIT_DEBUG("Not deleting underlying C++ object %p during V8 garbage collection\n", void_object);
	}
	bool destructive() const override {
//		std::cerr << fmt::format("_Delete::destructive") << std::endl;
		return false;
	}
	virtual std::string name() const override {

		return fmt::format("_leavealone");
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

	virtual v8::Local<v8::Object> operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const = 0;
};

// type to find the most derived type of an object and return a wrapped JavaScript object of that type
template<class, class, class = void>
struct WrapAsMostDerived;

template<class T>
struct WrapAsMostDerived<T, TypeList<>> : public WrapAsMostDerivedBase<T> {
	WrapAsMostDerived(v8::Isolate * isolate) : WrapAsMostDerivedBase<T>(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const override;
};


// specialization for when there is no const conflict
template<class T, class Head, class... Tail>
struct WrapAsMostDerived<T, TypeList<Head, Tail...>, std::enable_if_t<!std::is_const<T>::value || std::is_const<Head>::value>> : public WrapAsMostDerived<T, TypeList<Tail...>> {
	using SUPER = WrapAsMostDerived<T, TypeList<Tail...>>;
	WrapAsMostDerived(v8::Isolate * isolate) : SUPER(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const override;
};

// specializaiton for when we have something const (T) and want something non-const (Head)
template<class T, class Head, class... Tail>
struct WrapAsMostDerived<T, TypeList<Head, Tail...>, std::enable_if_t<std::is_const<T>::value && !std::is_const<Head>::value>> : public WrapAsMostDerived<T, TypeList<Tail...>> {
	using SUPER = WrapAsMostDerived<T, TypeList<Tail...>>;
	WrapAsMostDerived(v8::Isolate * isolate) : SUPER(isolate) {}
	virtual v8::Local<v8::Object> operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const override;
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



/**
 * Stores a list of callbacks to clean up V8ClassWrapper objects when the associated isolate is destroyed.
 * An isolate created after a previous isolate is destroyed may have the same address but a new wrapper must be
 * created.
 */
class V8ClassWrapperInstanceRegistry {
private:
	std::map<v8::Isolate *, std::vector<func::function<void()>>> isolate_to_callback_map;

public:
	void add_callback(v8::Isolate * isolate, func::function<void()> callback) {
		this->isolate_to_callback_map[isolate].push_back(callback);
	};
	void cleanup_isolate(v8::Isolate * isolate) {
//        std::cerr << fmt::format("cleaning up isolate: {}", (void*)isolate) << std::endl;
		for (auto & callback : this->isolate_to_callback_map[isolate]) {
			callback();
		}
		this->isolate_to_callback_map.erase(isolate);
	}
};

extern V8ClassWrapperInstanceRegistry wrapper_registery;





 // Cannot make class wrappers for pointer or reference types
#define V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE !std::is_pointer<T>::value && !std::is_reference<T>::value

#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX std::is_base_of<WrappedClassBase, T>::value
/**
 * Allows user to specify a list of types to instantiate real V8ClassWrapper template for -- CastToNative/CastToJS will otherwise
 *   try to instantiate it for a very large number of types which can drastically slow down compilation.
 * Setting an explicit value for this is NOT required - it is just a compile-time, compile-RAM (and maybe binary size) optimizatioan
 */


 // uncomment this to see the effects of generating the wrapper class on compile time (but won't actually run correctly)
// #define TEST_NO_REAL_WRAPPERS
 

#ifdef TEST_NO_REAL_WRAPPERS

#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<std::is_same<T, void>::value>
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<!std::is_same<T, void>::value>

 #else
// Use the real V8ClassWrapper specialization if the class inherits from WrappedClassBase or is in the user-provided sfinae
#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX)>

// otherwise use the 'cheap' specialization
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    !(V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX)>
#endif



/**
 * Constructor names already used, including things reserved by JavaScript like "Object" and "Number"
 */
extern std::map<v8::Isolate *, std::vector<std::string>> used_constructor_name_list_map;


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
template<class T, class = void>
class V8ClassWrapper;

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
	template<auto member> // type being returned
	static void _getter_helper(v8::Local<v8::Name> property,
							   v8::PropertyCallbackInfo<v8::Value> const & info) {

		auto isolate = info.GetIsolate();

		auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());

        // add lvalue ref as to know not to delete the object if the JS object is garbage collected
        info.GetReturnValue().Set(CastToJS<std::add_lvalue_reference_t<decltype(cpp_object->*member)>>()(isolate, cpp_object->*member));
    }


	/**
	 * Called when a JavaScript object's property is assigned to
	 * @param property property name
	 * @param value new value for property
	 * @param info general JavaScript state info
	 */
	template<auto member/*, std::enable_if_t<std::is_copy_assignable_v<pointer_to_member_t<member>>> = 0*/>
	static void _setter_helper(v8::Local<v8::Name> property,
							   v8::Local<v8::Value> value,
							   v8::PropertyCallbackInfo<void> const & info) {


	    auto isolate = info.GetIsolate();

        auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);


	    T * cpp_object = wrapper.get_cpp_object(info.Holder());
		using MemberT = std::remove_reference_t<decltype(cpp_object->*member)>;
        static_assert(
            std::is_copy_assignable_v<MemberT> ||
            std::is_move_assignable_v<MemberT>, "Cannot add_member with a type that is not either copy or move assignable.  Use add_member_readonly instead");

		// if it's copyable, then the assignment is pretty easy
		if constexpr(is_wrapped_type_v<MemberT>)
		{
			if constexpr(std::is_copy_assignable_v<MemberT>)
			{
				cpp_object->*member = CastToNative<MemberT &>()(isolate, value);
			}
			// otherwise if it's a wrapped type and it's move assignable, if the object owns its memory, then try a
			//   move assignment
			else if constexpr(std::is_move_assignable_v<MemberT>)
			{

				auto object = check_value_is_object(value, demangle<MemberT>());
				if (wrapper.does_object_own_memory(object)) {
					cpp_object->*member = CastToNative<MemberT &&>()(isolate, value);
				}
			}
		}
		// for an unwrapped type, always try to make a copy and do a move assignment from it
		else {
			cpp_object->*member = CastToNative<MemberT>()(isolate, value);
		}


	    // call any registered change callbacks
	    V8ClassWrapper<T>::get_instance(isolate).call_callbacks(info.Holder(), *v8::String::Utf8Value(property), value);
	}



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
     * Returns the embedded C++ object in a JavaScript object, or nullptr if the JavaScript object is either
     * a plain JavaScript object or is wrapping another, incompatible, C++ type
     * @param object JavaScript object containing an embedded C++ object
     * @return The embedded C++ object
     */
	T * get_cpp_object(v8::Local<v8::Object> object);


	/**
     * Experimental
     * @param callback
     */
    void register_callback(PropertyChangedCallback callback);
    
	/**
	 * Object still has the memory, it just doesn't own it.  It may or may not have owned it before, but now
	 * it doesn't.  For example, this is called passing an object to a C++ function taking a parameter type of a
	 * std::unique_ptr, since responsibility for the memory now belongs to the std::unique_ptr.  After calling this
	 * function, calling `does_object_own_memory` on the same object will always return `false`
	 * @param object JS object to make sure no longer owns its internal CPP object
	 * @return the cpp object from inside the provided JS object
	 */
	T * release_internal_field_memory(v8::Local<v8::Object> object) {
//		std::cerr << fmt::format("Trying to release internal field memory") << std::endl;
		auto wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
		WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());

        if (!this->does_object_own_memory(object)) {
            throw CastException("Cannot V8ClassWrapper::release_internal_field_memory because the object does not own its own memory");
        }

		if (!std::has_virtual_destructor<T>::value && dynamic_cast<AnyPtr<T> *>(wrapped_data->native_object) == nullptr) {
#ifdef ANYBASE_DEBUG
			std::cerr << fmt::format("cached anybase type: {} vs T: {}", wrapped_data->native_object->type_name, demangle<T>()) << std::endl;
#endif
			throw CastException("Tried to release internal field memory on the wrong type for a non-virtual-destructor type");
		}

		wrapped_data->weak_callback_data->global.ClearWeak();

		// since the weak callback won't run to delete the SetWeakCallbackData memory, do that now
		delete wrapped_data->weak_callback_data;
        wrapped_data->weak_callback_data = nullptr;

		return V8ClassWrapper<T>::get_cpp_object(object);
	}

    /**
     * Returns true if the destructor associated with the object has a destructive() static method which returns true.
     * In general, this means that when the object is garbage collected in JavaScript that the C++ object will be destroyed.
     * Some examples of when an "owning" JavaScript object is created are when creating the C++ object from JavaScript
     * or when turning a std::unique_ptr or rvalue reference into a JavaScript object.
     * @param object Object whose DestructorBehavior object is to be checked
     * @return whether the object "owns" the memory or not.
     */
    static bool does_object_own_memory(v8::Local<v8::Object> object) {
        auto wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
        WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
//		std::cerr << fmt::format("Does object own memory?  ptr: {}, weak callback data: {} type: {}, wrapped_data: {}", (void*) wrapped_data->original_pointer, (void*)wrapped_data->weak_callback_data, demangle<T>(), (void*)wrapped_data) << std::endl;
		if (wrapped_data->weak_callback_data) {
//			std::cerr << fmt::format("callback data destructive? {}", wrapped_data->weak_callback_data->destructive) << std::endl;
		}
        return wrapped_data->weak_callback_data != nullptr && wrapped_data->weak_callback_data->destructive;
    }

    /**
     * Sets up all the necessary state in a newly-created JavaScript object to support holding a wrapped C++ object
     * @param isolate isolate the Object will exist in
     * @param js_object newly-created JavaScript object
     * @param cpp_object the C++ object to be wrapped by the JavaScript object
     * @param destructor_behavior the DestructorBehavior to use for this object.
     */
	static void initialize_new_js_object(v8::Isolate * isolate,
										 v8::Local<v8::Object> js_object,
										 T * cpp_object,
										 DestructorBehavior const & destructor_behavior)
	{
#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "Initializing new js object for %s for v8::object at %p and cpp object at %p and any_ptr at %p\n", demangle<T>().c_str(), *js_object, cpp_object, (void*)any_ptr);
#endif
        if (js_object->InternalFieldCount() == 0) {
            fprintf(stderr, "Maybe you are calling a constructor without 'new'?");
        }
		assert(js_object->InternalFieldCount() >= 1);


		auto weak_callback_data = v8toolkit::global_set_weak(
			isolate,
			js_object,
			[isolate, cpp_object, &destructor_behavior](v8::WeakCallbackInfo<SetWeakCallbackData> const & info) {
				destructor_behavior(isolate, cpp_object);
			},
			destructor_behavior.destructive()
		);


		WrappedData<T> * wrapped_data = new WrappedData<T>(cpp_object, weak_callback_data);

#ifdef V8_CLASS_WRAPPER_DEBUG
		fprintf(stderr, "inserting anyptr<%s>at address %p pointing to cpp object at %p\n", typeid(T).name(), wrapped_data->native_object, cpp_object);
#endif


		js_object->SetInternalField(0, v8::External::New(isolate, wrapped_data));

		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));

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

    // Any cleanup for when an isolate is destroyed goes in here
	void isolate_about_to_be_destroyed(v8::Isolate * isolate) {
//        std::cerr << fmt::format("wrapper<{}> for isolate {} being destroyed", this->class_name, (void*)isolate) << std::endl;

        // forces object to be re-created next time an instance is requested
        isolate_to_wrapper_map.erase(isolate);

        // "global" names in the isolate also become available again
        used_constructor_name_list_map.erase(isolate);
	}

	
	/**
	* Returns a "singleton-per-isolate" instance of the V8ClassWrapper for the wrapped class type.
	* For each isolate you need to add constructors/methods/members separately.
	*/
    static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate) {

		auto wrapper_find_result = isolate_to_wrapper_map.find(isolate);
        if ( wrapper_find_result != isolate_to_wrapper_map.end()) {

			V8ClassWrapper<T> * wrapper = wrapper_find_result->second;
			return *wrapper;
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

    // allows specifying a special deleter type for objects which need to be cleaned up in a specific way
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
	v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object, DestructorBehavior & destructor_behavior, bool force_wrap_this_type = false)
	{
		// TODO: Expensive - when combined with add_method -- maybe try and move this out of V8ClassWrapper?
		auto isolate = this->isolate;
        assert(existing_cpp_object != nullptr);

        // if it's not finalized, try to find an existing CastToJS conversion because it's not a wrapped class
	    //*** IF YOU ARE HERE LOOKING AT AN INFINITE RECURSION CHECK THE TYPE IS ACTUALLY WRAPPED ***
	    if (!this->is_finalized()) {
            // fprintf(stderr, "wrap existing cpp object cast to js %s\n", typeid(T).name());
//            return CastToJS<T>()(isolate, *existing_cpp_object).template As<v8::Object>();
			throw CastException(fmt::format("Tried to wrap existing cpp object for a type that isn't finalized: {}", demangle<T>()));
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
                javascript_object = this->wrap_as_most_derived(existing_cpp_object, destructor_behavior);
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



	// this form is required for selecting between different overloaded instances of the same function name
	template<class R, class... Params, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name, R(*function)(Params...), DefaultArgs const default_args_tuple = DefaultArgs{}) {

		return add_static_method(method_name, function_type_t<decltype(function)>(function), default_args_tuple);
	};




	template<class Callable, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name, Callable callable, DefaultArgs const default_args_tuple = DefaultArgs{}) {
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


		auto static_method_adder = [this, method_name, callable, default_args_tuple](v8::Local<v8::FunctionTemplate> constructor_function_template) {

			// the function called is this capturing lambda, which calls the actual function being registered
			auto static_method_function_template =
				v8toolkit::make_function_template(this->isolate, [default_args_tuple, callable](const v8::FunctionCallbackInfo<v8::Value>& info) {

					auto callable_func = function_type_t<Callable>(callable);
					CallCallable<decltype(callable_func)>()(
						callable_func,
						info,
						get_index_sequence_for_func_function(callable_func),
						default_args_tuple);

				}, method_name);

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
		MemberType (MemberClass::*member)>
	void add_member(std::string const & member_name) {
		this->add_member<member>(member_name);
	}


	template<auto member>
	void add_member(std::string const & member_name) {
	    assert(this->finalized == false);



        // This function could forward the request to add_member_readonly instead, but having it error instead makes the
        //   caller be clear that they understand it's a const type.  If it turns out this is really annoying, it can be changed
        static_assert(!is_pointer_to_const_data_member_v<member>, "Cannot V8ClassWrapper::add_member a const data member.  Use add_member_readonly instead");

	    if constexpr(!std::is_const_v<T>) {
			V8ClassWrapper<ConstT>::get_instance(isolate).
				template add_member_readonly<member>(member_name);
	    }

	    this->check_if_name_used(member_name);

	    // store a function for adding the member on to an object template in the future
	    member_adders.emplace_back([this, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){


		    constructor_template->SetAccessor(v8::Local<v8::Name>::Cast(v8::String::NewFromUtf8(isolate, member_name.c_str())),
						      _getter_helper<member>,
						      _setter_helper<member>);
		});
	}


    template<class MemberType,
		class MemberClass, 	// allow members from parent types of T
		MemberType (MemberClass::*member),
		std::enable_if_t<std::is_base_of<MemberClass, T>::value, int> = 0>
	void add_member_readonly(std::string const & member_name) {
		this->add_member_readonly<member>(member_name);
	}

	template<auto member>
	void add_member_readonly(std::string const & member_name) {

	    // the field may be added read-only even to a non-const type, so make sure it's added to the const type, too
	    if (!std::is_const<T>::value) {
		    V8ClassWrapper<ConstT>::get_instance(isolate).template add_member_readonly<member>(member_name);
	    }

	    assert(this->finalized == false);

	    this->check_if_name_used(member_name);

	    member_adders.emplace_back([this, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){

		    constructor_template->SetAccessor(v8::String::NewFromUtf8(isolate, member_name.c_str()),
						      _getter_helper<member>,
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


	v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object, DestructorBehavior & destructor_behavior) {
		return this->wrap_as_most_derived_object->operator()(cpp_object, destructor_behavior);
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
	void _add_method(std::string const & method_name,
                                    M method,
                                    TypeList<Args...> const &,
                                    std::tuple<DefaultArgTypes...> const & default_args_tuple,
                                    bool add_as_callable_object_callback = false) {
		assert(this->finalized == false);

		this->check_if_name_used(method_name);


		// adds this method to the list of methods to be added when a new "javascript constructor"
		// is created for the type being wrapped
		MethodAdderData method_adder_data =
			MethodAdderData{method_name, StdFunctionCallbackType(

				// This is the function called (via a level of indirection because this is a capturing lambda)
				//   when the JavaScript method is called.
				[this, default_args_tuple, method, method_name]
					(const v8::FunctionCallbackInfo<v8::Value> & info) {
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
					for (auto & function_template : this->this_class_function_templates) {
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
						V8TOOLKIT_DEBUG(
							"No match in prototype chain after looking through all potential function templates\n");
						assert(false);
					}


					// void* pointer = instance->GetAlignedPointerFromInternalField(0);

					auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));

//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "uncasted internal field: %p\n", wrap->Value());
					WrappedData<T> * wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
					auto backing_object_pointer = V8ClassWrapper<T>::get_instance(isolate).cast(
						wrapped_data->native_object);

//			    assert(backing_object_pointer != nullptr);
					// bind the object and method into a func::function then build the parameters for it and call it
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "binding with object %p\n", backing_object_pointer);
					auto bound_method = v8toolkit::bind<T>(*backing_object_pointer, method);



					// V8 does not support C++ exceptions, so all exceptions must be caught before control
					//   is returned to V8 or the program will instantly terminate
					try {
						// make a copy of default_args_tuple so it's non-const - probably better to do this on a per-parameter basis
						CallCallable<decltype(bound_method)>()(bound_method, info,
															   std::index_sequence_for<Args...>{},
															   std::tuple<DefaultArgTypes...>(
																   default_args_tuple));
					} catch (std::exception & e) {
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

#if 0
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

#endif

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




template<class T>
struct CastToJS<T, std::enable_if_t<is_wrapped_type_v<T>>> {

	// An lvalue is presented, so the memory will not be cleaned up by JavaScript
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object) {
		auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return wrapper.wrap_existing_cpp_object(isolate->GetCurrentContext(), &cpp_object,
										 *wrapper.destructor_behavior_leave_alone);
	}

	// An rvalue is presented, so move construct a new object from the presented data
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object) {
		auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
		return wrapper.wrap_existing_cpp_object(isolate->GetCurrentContext(), new T(std::move(cpp_object)),
												*wrapper.destructor_behavior_delete);
	}
};


/**
 * CastToNative a std::unique_ptr to a wrapped type
 */
template<class T, class... Rest>
struct CastToJS<std::unique_ptr<T, Rest...>, std::enable_if_t<is_wrapped_type_v<T>>> {

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> & unique_ptr) {
        return CastToJS<T>()(isolate, *unique_ptr.get());
    }

    v8::Local<v8::Value> operator()(v8::Isolate *isolate, std::unique_ptr<T, Rest...> && unique_ptr) {


//		std::cerr << fmt::format("cast to js {} type {}", (void*)unique_ptr.get(), demangle<T>()) << std::endl;
        auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);

        // create new owning JavaScript object with the contents of the unique_ptr
//		std::cerr << fmt::format("creating object from unique ptr with destructor behavior {}: {} {}", wrapper.destructor_behavior_delete->name(), (void*)wrapper.destructor_behavior_delete.get(), wrapper.destructor_behavior_delete->destructive()) << std::endl;
        auto result = wrapper.wrap_existing_cpp_object(isolate->GetCurrentContext(), unique_ptr.release(), *wrapper.destructor_behavior_delete);
//		std::cerr << fmt::format("Immediately after creation from unique_ptr, does own memory?  {} ", V8ClassWrapper<T>::does_object_own_memory(result)) << std::endl;
		auto wrap = v8::Local<v8::External>::Cast(result->ToObject()->GetInternalField(0));
		WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
//		std::cerr << fmt::format("WrappedData: {}", (void*)wrapped_data) << std::endl;
		return result;

    }
};





template<class T>
struct CastToJS<T *, std::enable_if_t<is_wrapped_type_v<T>>> {

    // An lvalue is presented, so the memory will not be cleaned up by JavaScript
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const cpp_object) {
        if (cpp_object == nullptr) {
            return v8::Undefined(isolate);
        }

        assert(cpp_object != (void *) 0xbebebebebebebebe);

        V8TOOLKIT_DEBUG("CastToJS from T* %s\n", demangle_typeid_name(typeid(T).name()).c_str());
        auto context = isolate->GetCurrentContext();
        V8ClassWrapper <T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
        using JSWrapperType = JSWrapper<std::remove_const_t<T>>;
//		fprintf(stderr, "Checking to see if object * is a JSWrapper *\n");

        if (std::is_const<T>::value) {
            auto js_wrapper = safe_dynamic_cast<JSWrapperType const *>(cpp_object);
            if (js_wrapper) {
                return CastToJS<const JSWrapperType>()(isolate, *js_wrapper);
            }
        } else {
            // this only runs if it's non-const, so casting is not scary - only to trick compiler
            using NonConstT = std::remove_const_t<T>;
            auto js_wrapper = safe_dynamic_cast<JSWrapperType *>(const_cast<NonConstT *>(cpp_object));
            if (js_wrapper) {
                return CastToJS<JSWrapperType>()(isolate, *js_wrapper);
            }
        }

#endif
        V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

        /** If you are here looking for an INFINITE RECURSION make sure the type is wrapped **/
        return class_wrapper.template wrap_existing_cpp_object(context, cpp_object,
                                                               *class_wrapper.destructor_behavior_leave_alone);
    }
};


template<class T>
struct CastToJS<T&, std::enable_if_t<is_wrapped_type_v<T>>> {

	// An lvalue is presented, so the memory will not be cleaned up by JavaScript
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object) {
		return 	CastToJS<T*>()(isolate, &cpp_object);
	}
};


template<class T>
struct CastToJS<T&&, std::enable_if_t<is_wrapped_type_v<T>>> {

	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object) {
		return CastToJS<std::unique_ptr<T>>()(isolate, std::make_unique<T>(std::move(cpp_object)));
	}
};


template<typename T>
struct CastToNative<T, std::enable_if_t<!std::is_const_v<T> && std::is_copy_constructible<T>::value && is_wrapped_type_v<T>>>
{
	T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		return T(*CastToNative<T*>()(isolate, value));
	}
	static constexpr bool callable(){return true;}
};


template<typename T>
struct CastToNative<T, std::enable_if_t<!std::is_copy_constructible<T>::value && is_wrapped_type_v<T>>>
{
	template<class U = T> // just to make it dependent so the static_asserts don't fire before `callable` can be called
	T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
		static_assert(always_false_v<T>, "Cannot return a copy of an object of a type that is not copy constructible");
	}
	static constexpr bool callable(){return false;}

};


template<typename T>
struct CastToNative<T&, std::enable_if_t<is_wrapped_type_v<T>>>
{
	T& operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		return *CastToNative<T*>()(isolate, value);
	}
	static constexpr bool callable(){return true;}
};


template<typename T>
struct CastToNative<T&&, std::enable_if_t<is_wrapped_type_v<T>>>
{
	// to "give permission" to have the object moved out of, this object must own the memory.  It CANNOT
	//   release ownership of the memory for the object, just the data in the object
	// TODO: Is this the right model?  This seems the safest policy, but is technically restricting of potentially valid operations
	T&& operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		v8::Local<v8::Object> object = check_value_is_object(value, demangle<T>());
		T * cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(object);
		if (V8ClassWrapper<T>::does_object_own_memory(object)) {
			return std::move(*cpp_object); // but do not release the memory
		}
		throw CastException("Could not cast object to {} && because it doesn't own it's memory", demangle<T>());
	}
	static constexpr bool callable(){return true;}
};


template<typename T>
struct CastToNative<T*, std::enable_if_t<is_wrapped_type_v<T>>>
{
	T* operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
		v8::Local<v8::Object> object = check_value_is_object(value, demangle<T>());

		return wrapper.get_cpp_object(object);
	}
	static constexpr bool callable(){return true;}
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
	v8::Local<v8::Object> object = check_value_is_object(value, demangle<T>());

	if (object->InternalFieldCount() <= 0) {
		throw CastException(fmt::format("No specialization CastToNative<{}> found (for any shortcut notation) and provided Object is not a wrapped C++ object.  It is a native Javascript Object", demangle<T>()));
	}
	v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
	auto wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());

//	 std::cerr << fmt::format("about to call cast on {}", demangle<T>()) << std::endl;
	T * t;
	if ((t = V8ClassWrapper<T>::get_instance(isolate).cast(wrapped_data->native_object)) == nullptr) {
		fprintf(stderr, "Failed to convert types: want:  %d %s\n", std::is_const<T>::value, typeid(T).name());
		throw CastException(fmt::format("Cannot convert AnyBase to {}", demangle<T>()));
	}
//		std::cerr << fmt::format("Successfully converted") << std::endl;
	return *t;
}

template<class T, class... Rest>
struct CastToNative<std::unique_ptr<T, Rest...>, std::enable_if_t<is_wrapped_type_v<T>>>
{
	std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
		HANDLE_FUNCTION_VALUES;

		auto object = check_value_is_object(value, demangle<T>());

		// if the javascript object owns the memory, it gives up that ownership to the unique_ptr
		if (V8ClassWrapper<T>::does_object_own_memory(object)) {
			auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
			return std::unique_ptr<T, Rest...>(wrapper.release_internal_field_memory(object));
		} else if constexpr(std::is_copy_constructible_v<T>) {
			T & cpp_object = get_object_from_embedded_cpp_object<T>(isolate, value);
			return std::unique_ptr<T, Rest...>(new T(cpp_object));
		} else {
			throw CastException("Cannot CastToNative<unique_ptr<{}>> from object that doesn't own it's memory and isn't copy constructible: {}",
				demangle<T>(), *v8::String::Utf8Value(value));
		}
	}
	static constexpr bool callable(){return true;}

};

// If no more-derived option was found, wrap as this type
template<class T>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<>>::operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const {
	auto context = this->isolate->GetCurrentContext();

		// TODO: Expensive
	auto & wrapper = v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	return wrapper.template wrap_existing_cpp_object(context, cpp_object, destructor_behavior, true /* don't infinitely recurse */);
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
::operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const {

	// if they're the same, let it fall through to the empty typechecker TypeList base case
	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
		using MatchingConstT = std::conditional_t<std::is_const<Head>::value, std::add_const_t<T>, std::remove_const_t<T>>;

		if (std::is_const<T>::value == std::is_const<Head>::value) {
			if (auto derived = safe_dynamic_cast<Head *>(const_cast<MatchingConstT *>(cpp_object))) {
				return v8toolkit::V8ClassWrapper<Head>::get_instance(this->isolate).wrap_as_most_derived(derived, destructor_behavior);
			}
		}
	}
	return SUPER::operator()(cpp_object, destructor_behavior);
}



template <class T>
struct ParameterBuilder<T, std::enable_if_t<std::is_reference_v<T> && is_wrapped_type_v<std::remove_reference_t<T>>> > {

    using NoRefT = std::remove_reference_t<T>;
	using NoConstRefT = std::remove_const_t<NoRefT>;

    template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
	T /*T& or T&&*/ operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
					std::vector<std::unique_ptr<StuffBase>> & stuff,
					DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
		PB_PRINT("ParameterBuilder handling wrapped type: {} {}", demangle<T>(),
				 std::is_rvalue_reference_v<T> ? "&&" : "&");
		auto isolate = info.GetIsolate();

		if (i >= info.Length()) {
			return std::forward<T>(*get_default_parameter<NoConstRefT, default_arg_position>(info, i, stuff, default_args_tuple));
		} else {

			auto & wrapper = V8ClassWrapper<NoRefT>::get_instance(isolate);
			auto value = info[i++];

			if (value->IsObject()) {
				auto object = value->ToObject();
				if (auto cpp_object = wrapper.get_instance(isolate).get_cpp_object(object)) {

					if constexpr(std::is_rvalue_reference_v<T>)
					{
						if (wrapper.does_object_own_memory(object)) {
							return std::move(*cpp_object);
						} else if constexpr(std::is_copy_constructible_v<NoConstRefT>)
						{
							// make a copy, put it in stuff, and return an rvalue ref to the copy
							stuff.emplace_back(
								std::make_unique<Stuff<NoConstRefT>>(std::make_unique<NoRefT>(*cpp_object)));
							return std::forward<T>(*(static_cast<Stuff<NoConstRefT> &>(*stuff.back()).get()));
						}
					}
					// as a policy, only do this if it's an lvalue reference requested
					else {
						return *cpp_object;
					}
				}
			}
			if constexpr(std::is_move_constructible_v<NoConstRefT> && CastToNative<NoConstRefT>::callable())
			{
				stuff.emplace_back(std::make_unique<Stuff<NoConstRefT>>(CastToNative<NoConstRefT>()(isolate, value)));
				return std::forward<T>(*(static_cast<Stuff<NoConstRefT> &>(*stuff.back()).get()));
			}
			throw CastException("Could not create requested object of type: {} {}.  Maybe you don't 'own' your memory?",
								demangle<T>(),
								std::is_rvalue_reference_v<T> ? "&&" : "&");
		}
	}
};





template <class T>
struct ParameterBuilder<T, std::enable_if_t<std::is_copy_constructible_v<T> && is_wrapped_type_v<T> > > {

	template<int default_arg_position = -1, class DefaultArgsTuple = std::tuple<>>
	T operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
				 std::vector<std::unique_ptr<StuffBase>> & stuff,
				 DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
		PB_PRINT("ParameterBuilder handling wrapped type: {}", demangle<T>());
		auto isolate = info.GetIsolate();
		auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);

		if (i >= info.Length()) {
			return *get_default_parameter<T, default_arg_position>(info, i, stuff, default_args_tuple);
		} else {
			auto value = info[i++];
			if (value->IsObject()) {
				auto object = value->ToObject();
				if (auto cpp_object = wrapper.get_instance(isolate).get_cpp_object(object)) {
					// make a copy, put it in stuff, and return an rvalue ref to the copy
					return *cpp_object;
				}
			}
			return CastToNative<T>()(isolate, value);
		}
	}
};





} // namespace v8toolkit






































































































































































































































































#if 0

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
		using JSWrapperType = JSWrapp     er<std::remove_const_t<T>>;
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

		/** If you are here looking for an INFINITE RECURSION make sure the type is wrapped *
		return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
	}
};
//
//template<typename T>
//struct CastToJS<T*, std::enable_if_t<!std::is_polymorphic<T>::value>> {
//	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){
//
//	    if (cpp_object == nullptr) {
//		return v8::Local<v8::Object>();
//	    }
//	    assert(cpp_object != (void *)0xbebebebebebebebe);
//
//
//	    V8TOOLKIT_DEBUG("CastToJS from T* %s\n", demangle<T>().c_str());
//		auto context = isolate->GetCurrentContext();
//		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
//
//		V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());
//
//		return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
//	}
//
// };
//

// Pointers to wrapped types
 template<typename T>
     struct CastToJS<T, std::enable_if_t<std::is_pointer<T>::value && is_wrapped_type_v<T>>> {
     v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const cpp_object){

		 if (cpp_object == nullptr) {
			 return v8::Local<v8::Object>();
		 }
         assert(cpp_object != (void *)0xbebebebebebebebe);

         V8TOOLKIT_DEBUG("CastToJS from T* {}\n", demangle<T>().c_str());
         auto context = isolate->GetCurrentContext();
         auto & class_wrapper = V8ClassWrapper<std::remove_pointer_t<T>>::get_instance(isolate);

         V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

         return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
     }
 };
//
//
//    template<typename T>
//    struct CastToJS<T const * const> {
//        v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * cpp_object){
//
//            if (cpp_object == nullptr) {
//                return v8::Local<v8::Object>();
//            }
//            assert(cpp_object != (void *)0xbebebebebebebebe);
//
//            V8TOOLKIT_DEBUG("CastToJS from T* {}\n", demangle<T>().c_str());
//            auto context = isolate->GetCurrentContext();
//            auto & class_wrapper = V8ClassWrapper<T const>::get_instance(isolate);
//
//            V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());
//
//            return class_wrapper.template wrap_existing_cpp_object(context, cpp_object, *class_wrapper.destructor_behavior_leave_alone);
//        }
//    };

//static_assert(std::is_same<T, T*>::value, "Cannot CastToJS a pointer type not wrapped with V8ClassWrapper");

template<typename T>
struct CastToJS<T, std::enable_if_t<is_wrapped_type_v<T>>> {


	v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_reference_t<T> & cpp_object){
		V8TOOLKIT_DEBUG("CastToJS from lvalue ref %s\n", demangle<T>().c_str());
		return CastToJS<typename std::add_pointer<T>::type>()(isolate, &cpp_object);
	}

	/**
	* If an rvalue is passed in, a copy must be made.
	*/
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, std::remove_reference_t<T> && cpp_object){
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


template<typename T>
struct CastToJS<T*&> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object) {
		return CastToJS<T*>()(isolate, cpp_object);
	}
};


template<typename T>
struct CastToJS<T const *&> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * cpp_object) {
		return CastToJS<T const *>()(isolate, cpp_object);
	}
};


template<typename T>
struct CastToJS<T* const &> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const cpp_object) {
		return CastToJS<T * const>()(isolate, cpp_object);
	}
};


template<typename T>
struct CastToJS<T const * const &> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * const cpp_object) {
		return CastToJS<T const * const>()(isolate, cpp_object);
	}
};

#endif





