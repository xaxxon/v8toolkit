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
#include <functional>

#include <xl/demangle.h>
#include <xl/member_function_type_traits.h>

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
#include "any.h"
#include "type_traits.h"

// allow _v8 suffix for making v8::String objects
using namespace v8toolkit::literals;

// this enables certain functionality only if bidirectional.h has been included
#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
#define V8_CLASS_WRAPPER_HAS_BIDIRECTIONAL_SUPPORT
#endif

class Target;


namespace v8toolkit {


//#define V8_CLASS_WRAPPER_DEBUG

#ifdef V8_CLASS_WRAPPER_DEBUG
#define V8TOOLKIT_DEBUG(format_string, ...) \
    fprintf(stderr, format_string, ##__VA_ARGS__);
#else
#define V8TOOLKIT_DEBUG(format_string, ...)
#endif


/***
* set of classes for determining what to do do the underlying c++ object when the javascript object is garbage collected
*/
struct DestructorBehavior
{
    virtual ~DestructorBehavior(){}

	virtual void operator()(v8::Isolate * isolate, const void * object) const = 0;
	virtual void operator()(v8::Isolate * isolate, const volatile void * object) const = 0;

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

	void operator()(v8::Isolate * isolate, const volatile void * void_object) const override {
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
	void operator()(v8::Isolate * isolate, const volatile void * void_object) const override {
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


/**
 * Takes an AnyBase object and determines if it contains an object of type T
 */
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


/**
 * Takes a C++ object and returns the most derived (specific) type it is based on an explicit list
 * of known derived types
 */
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




/**
 * Checks to see if an object of type AnyBase contains an object of type BaseType by seeing if
 * it can be dynamic_cast'd to any of the types in TypeListDerivedTypes
 *
 * Example: class A; class B : public A; class C : public A;
 * TypeChecker<A, TypeList<A, B, C>> checks to see if the AnyBase contains an A, B, or C object
 * and returns that object as an A if it does.
 */
template<class BaseType, class TypeListDerivedTypes, class = void>
struct TypeChecker;


/**
 * TypeChecker specialization when there are no types to check against
 */
template<class T>
struct TypeChecker<T, TypeList<>> : public TypeCheckerBase<T>
{
	TypeChecker(v8::Isolate * isolate) : TypeCheckerBase<T>(isolate) {}
	virtual T * check(AnyBase * any_base, bool first_call = true) const override {
		// No types left to try, so return nullptr to signal that no matching type
		//   was found
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
//		ANYBASE_PRINT("In Type Checker<{}> Const mismatch: {} (string: {})", xl::demangle<T>(), xl::demangle<Head>(), any_base->type_name);
        if (dynamic_cast<AnyPtr<Head> *>(any_base) != nullptr) {
//            ANYBASE_PRINT("Not a match, but the value is a const version of goal type! {} vs {} Should you be casting to a const type instead?",  xl::demangle<T>(),  xl::demangle<Head>());
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
	MapT<v8::Isolate *, std::vector<func::function<void()>>> isolate_to_callback_map;

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


// By default, is_wrapped_type type trait defines which classes are wrapped, but user may redefine criteria
//   it is normally preferred to specialize is_wrapped_type for additional types rather than override this #define
#ifndef V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX
#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX v8toolkit::is_wrapped_type_v<T>
#endif


#define V8TOOLKIT_V8CLASSWRAPPER_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX)>

/**
 * Constructor names already used, including things reserved by JavaScript like "Object" and "Number"
 */
inline MapT<v8::Isolate *, std::vector<std::string>> used_constructor_name_list_map;




/**
 * Holds the c++ object to be embedded inside a javascript object along with additional debugging information
 *   when requested
 */
template<class T>
struct WrappedData {

	// holds the wrapped C++ object such that the actual type can be determined at runtime
	AnyBase * native_object;

	// the callback associated with the JS GC such that it can be changed if necessary
	SetWeakCallbackData * weak_callback_data = nullptr;

	WrappedData(T * native_object,
				SetWeakCallbackData * weak_callback_data) :
		native_object(new AnyPtr<T>(native_object)),
		weak_callback_data(weak_callback_data)
	{
		std::cerr << fmt::format("created WrappedData<{}> with native_object = {} - this: {}\n", xl::demangle<T>(), (void*)native_object, (void*)this);
		if (native_object == nullptr) {
			std::cerr << fmt::format("problem here\n");
		}
	}

	~WrappedData(){delete native_object;}

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
template<class T, class = void>
class V8ClassWrapper {
	static_assert(std::is_same_v<T*, void>, "Tried to instantiate a V8ClassWrapper with a type for which is_wrapped_type_v<T> is false");
};


template<class T>
class V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_TEMPLATE_SFINAE>
{
	using ConstT = std::add_const_t<T>;

private:

	/*** TYPEDEFS ***/

    // Callback type to add members to an ObjectTemplate
	using AttributeAdder = func::function<void(v8::Local<v8::ObjectTemplate> &)>;
	using EnumAdder = func::function<void(v8::Local<v8::ObjectTemplate> &)>;

    // Callback type to add a static method or member to a FunctionTemplate
	using StaticAdder = func::function<void(v8::Local<v8::FunctionTemplate>)>;

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
	std::vector<EnumAdder> enum_adders;

    // Callbacks for adding static methods to an ObjectTemplate
	std::vector<StaticAdder> static_adders;


	/// List of callbacks for when attributes change
	std::vector<PropertyChangedCallback> property_changed_callbacks;


	// stores callbacks to add calls to lambdas whos first parameter is of type T* and are automatically passed
	//   the "this" pointer before any javascript parameters are passed in
	std::vector<FakeMethodAdder> fake_method_adders;

	/**
	 * Name that will be reported from JavaScript `typeof` function
	 */
	std::string class_name = xl::demangle<T>();

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

	MapT<std::string, MapT<std::string, double>> enums;

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
    void check_if_name_used(std::string_view name);


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
	template<auto member_getter>
	static void _getter_helper(v8::Local<v8::Name> property,
							   v8::PropertyCallbackInfo<v8::Value> const & info) {


		auto isolate = info.GetIsolate();

		auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());
        log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "got {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);
		auto non_const_cpp_object = const_cast<std::remove_const_t<T>*>(cpp_object);
		using MemberT = decltype(member_getter(non_const_cpp_object));


		log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS,
				 "Reading data member of type {} (need to implement having the name available)",
				 xl::demangle<MemberT>());

		// add lvalue ref as to know not to delete the object if the JS object is garbage collected
        info.GetReturnValue().Set(CastToJS<std::add_lvalue_reference_t<MemberT>>()(isolate, member_getter(non_const_cpp_object)));
    }




	/**
	 * Called when a JavaScript object's property is assigned to
	 * @param property property name
	 * @param value new value for property
	 * @param info general JavaScript state info
	 */
	template<auto member_getter/*, std::enable_if_t<std::is_copy_assignable_v<pointer_to_member_t<member>>> = 0*/>
	static void _setter_helper(v8::Local<v8::Name> property,
							   v8::Local<v8::Value> value,
							   v8::PropertyCallbackInfo<void> const & info) {


	    auto isolate = info.GetIsolate();

        auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);


	    T * cpp_object = wrapper.get_cpp_object(info.Holder());
		log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "setter got {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);

		using MemberT = std::remove_reference_t<decltype(member_getter(cpp_object))>;
		using DereferencedMemberT = xl::dereferenced_type_t<MemberT>;

		log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS,
				 "Setting data member of type {} (need to implement having the name available)",
				 xl::demangle<MemberT>());

		
        static_assert(
        	std::is_pointer_v<MemberT> ||
            std::is_copy_assignable_v<MemberT> ||
            std::is_move_assignable_v<MemberT>, "Cannot add_member with a type that is not either copy or move assignable.  Use add_member_readonly instead");

		// if it's copyable, then the assignment is pretty easy
		if constexpr(is_wrapped_type_v<MemberT>) {
			if constexpr(std::is_copy_assignable_v<MemberT>)
			{
				member_getter(cpp_object) = CastToNative<MemberT &>()(isolate, value);
			}
			// otherwise if it's a wrapped type and it's move assignable, if the object owns its memory, then try a
			//   move assignment
			else if constexpr(std::is_move_assignable_v<MemberT>)
			{

				auto object = get_value_as<v8::Object>(isolate, value);
				if (wrapper.does_object_own_memory(object)) {
					member_getter(cpp_object) = CastToNative<MemberT &&>()(isolate, value);
				}
			}
		}
		// for an unwrapped type, always try to make a copy and do a move assignment from it
		else {
			static_assert(is_wrapped_type_v<DereferencedMemberT> || !std::is_pointer_v<MemberT>,
						  "Cannot assign to a non-wrapped type pointer - Who would own the memory?");

			if constexpr(is_wrapped_type_v<DereferencedMemberT> || !std::is_pointer_v<MemberT>) {

				auto native_value = CastToNative<MemberT>()(isolate, value);
				member_getter(cpp_object) = std::move(native_value);
			}
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

		// Store a function which calls the constructor given the correct arguments
		func::function<void(CONSTRUCTOR_PARAMETER_TYPES...)> constructor =
				[&new_cpp_object](CONSTRUCTOR_PARAMETER_TYPES... info)->void{new_cpp_object = new T(std::forward<CONSTRUCTOR_PARAMETER_TYPES>(info)...);};

		// route any cpp exceptions through javascript
		try {
			CallCallable<decltype(constructor)>()(constructor,
												  info,
												  std::make_integer_sequence<int, sizeof...(CONSTRUCTOR_PARAMETER_TYPES)>{},
												  DefaultArgsTupleType(*default_args_tuple_ptr));

		} catch(std::exception & e) {

			if constexpr(constructed_from_callback_info_v<T>) {
				std::cerr << fmt::format("attempting CallbackInfo constructor because {}", e.what()) << std::endl;
				new_cpp_object = new T(info);
			} else {

				log.error(LoggingSubjects::Subjects::RUNTIME_EXCEPTION,
						  "Exception while running C++ constructor for {}: {}",
						  xl::demangle<T>(), e.what());
				isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
				return;
			}
		}


		// if the object was created by calling new in javascript, it should be deleted when the garbage collector
		//   GC's the javascript object, there should be no c++ references to it
		auto & deleter = *v8toolkit::V8ClassWrapper<T>::get_instance(isolate).destructor_behavior_delete;

		get_instance(isolate).initialize_new_js_object(isolate, info.This(), new_cpp_object, deleter);

		// // return the object to the javascript caller
		info.GetReturnValue().Set(info.This());
	}

	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args);


	// returns the WrappedData<T> object from the InternalField inside the provided object, or nullptr if
	//   not available
	static WrappedData<T> & get_wrapped_data(v8::Local<v8::Object> object);

public:




	std::string get_class_details_string() {
		std::stringstream result;
		result << fmt::format("V8ClassWrapper<{}>\n",  xl::demangle<T>()) << std::endl;
		result << fmt::format("finalized: {}", this->finalized) << std::endl;
		result << fmt::format("Constructing FunctionTemplates created: {}", this->this_class_function_templates.size()) << std::endl;
		result << fmt::format("methods added: {}", this->method_adders.size()) << std::endl;
		result << fmt::format("static elements added: {}", this->static_adders.size()) << std::endl;
		result << fmt::format("data members added: {}", this->member_adders.size()) << std::endl;
		result << fmt::format("property changed callbacks registered: {}", this->property_changed_callbacks.size()) << std::endl;
		return result.str();
	}



	DestructorBehavior * destructor_behavior_delete = new DestructorBehavior_Delete<T>(); // unique_ptr too expensive to compile
	DestructorBehavior * destructor_behavior_leave_alone = new DestructorBehavior_LeaveAlone(); // unique_ptr too expensive to compile


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


		if (!this->does_object_own_memory(object)) {
			throw CastException(
				"Cannot V8ClassWrapper::release_internal_field_memory because the object does not own its own memory");
		}

		auto & wrapped_data = this->get_wrapped_data(object);


		// if the type doesn't have a virtual destructor, the object can't be released as a type T unless that's the most derived type, since
		//   the wrong destructor would run
		if constexpr(!std::has_virtual_destructor_v<T>) {
			if (dynamic_cast<AnyPtr<T> *>(wrapped_data.native_object) == nullptr) {
				CastException(
					"Tried to release internal field memory on the wrong type for a non-virtual-destructor type");
			}
		}

		wrapped_data.weak_callback_data->global.ClearWeak();

		// since the weak callback won't run to delete the SetWeakCallbackData memory, do that now
		delete wrapped_data.weak_callback_data;
		wrapped_data.weak_callback_data = nullptr;

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
        WrappedData<T> & wrapped_data = get_wrapped_data(object);

//		std::cerr << fmt::format("Does object own memory?  ptr: {}, weak callback data: {} type: {}, wrapped_data: {}",
//                                 (void*)wrapped_data.native_object,
//                                 (void*)wrapped_data.weak_callback_data,
//                                 xl::demangle<T>(),
//                                 (void*)&wrapped_data) << std::endl;
//
//		if (wrapped_data.weak_callback_data) {
//			std::cerr << fmt::format("callback data destructive? {}", wrapped_data.weak_callback_data->destructive) << std::endl;
//		}
        bool result =  wrapped_data.weak_callback_data != nullptr && wrapped_data.weak_callback_data->destructive;

        return result;
    }

    /**
     * Sets up all the necessary state in a newly-created JavaScript object to support holding a wrapped C++ object
     * @param isolate isolate the Object will exist in
     * @param js_object newly-created JavaScript object
     * @param cpp_object the C++ object to be wrapped by the JavaScript object
     * @param destructor_behavior the DestructorBehavior to use for this object.
     */
	void initialize_new_js_object(v8::Isolate * isolate,
										 v8::Local<v8::Object> js_object,
										 T * cpp_object,
										 DestructorBehavior const & destructor_behavior)
	{
#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "Initializing new js object for %s for v8::object at %p and cpp object at %p\n",  xl::demangle<T>().c_str(), *js_object, cpp_object);
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


		auto wrapped_data = new WrappedData<T>(cpp_object, weak_callback_data);

#ifdef V8_CLASS_WRAPPER_DEBUG

		fprintf(stderr, "inserting anyptr<%s>at address %p pointing to cpp object at %p\n", typeid(T).name(), wrapped_data->native_object, cpp_object);
#endif


		js_object->SetInternalField(0, v8::External::New(isolate, wrapped_data));

		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));

		this->existing_wrapped_objects.emplace(cpp_object,
											   v8::Global<v8::Object>(isolate, js_object));

		V8TOOLKIT_DEBUG("Inserting new %s object at %p into existing_wrapped_objects hash that is now of size: %d\n",  xl::demangle<T>().c_str(), cpp_object, (int)this->existing_wrapped_objects.size());


	}


    /**
    * Creates a new v8::FunctionTemplate capabale of creating wrapped T objects based on previously added methods and members.
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
		using ConstT = std::add_const_t<T>;

        if constexpr(!std::is_const<T>::value && is_wrapped_type_v<ConstT>) {
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
		if constexpr(!std::is_const_v<T> && is_wrapped_type_v<ConstT>) {
			V8ClassWrapper<ConstT>::get_instance(this->isolate).template set_deleter<Deleter>();
		}
        delete this->destructor_behavior_delete;
		this->destructor_behavior_delete = new Deleter<T>();
	}


    /**
     * This wrapped class will inherit all the methods from the parent type (and its parent...)
     *
     * It is VERY important that the type being marked as the parent type has this type set with
     *   set_compatible_types<>()
     */
    template<class ParentType>
    void set_parent_type() {
        static_assert(std::is_base_of_v<ParentType, T>, "ParentType is not parent of type");
		assert(!is_finalized());

		using ConstT = std::add_const_t<T>;
		using ConstParentT = std::add_const_t<ParentType>;

		if constexpr(!std::is_const<T>::value && is_wrapped_type_v<ConstT> && is_wrapped_type_v<ConstParentT>) {
			V8ClassWrapper<ConstT>::get_instance(isolate).template set_parent_type<ConstParentT>();
		}



		if (!V8ClassWrapper<ParentType>::get_instance(isolate).is_finalized()) {
			fprintf(stderr, "Tried to set parent type of %s to unfinalized %s\n",
					 xl::demangle<T>().c_str(),  xl::demangle<ParentType>().c_str());

		}
		assert(V8ClassWrapper<ParentType>::get_instance(isolate).is_finalized());
//	fprintf(stderr, "Setting parent of %s to %s\n",  xl::demangle<T>().c_str(),  xl::demangle<ParentType>().c_str());
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
			throw V8Exception(args.GetIsolate(), "You cannot create an object of type: " + xl::demangle<T>());
		    },
		    v8::Local<v8::Value>());

	    // Add the constructor function to the parent object template (often the global template)
//	    std::cerr << "Adding static-method holder (non-constructor) to global with name: " << js_name << std::endl;
	    parent_template->Set(v8::String::NewFromUtf8(isolate, js_name.c_str()), non_constructor_template);
	}
	
	template<typename MemberType, typename C, typename DefaultArgs = std::tuple<>>
	void add(std::string_view name, MemberType C::*member, DefaultArgs default_args = DefaultArgs{}) {
        if constexpr(std::is_function_v<MemberType>) {
            add_method(name, member, default_args);
        } else {
            add_member<MemberType C::*>(name);
        }
    };


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
			throw CastException(fmt::format("Tried to wrap existing cpp object for a type that isn't finalized: {}",  xl::demangle<T>()));
        }

#ifdef V8_CLASS_WRAPPER_DEBUG
        fprintf(stderr, "Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p type: %s\n", existing_cpp_object, this, isolate, v8toolkit:: xl::demangle<T>().c_str());
#endif

		// if there's currently a javascript object wrapping this pointer, return that instead of making a new one
        //   This makes sure if the same object is returned multiple times, the javascript object is also the same
		v8::Local<v8::Object> javascript_object;
		if(this->existing_wrapped_objects.find(existing_cpp_object) != this->existing_wrapped_objects.end()) {
			V8TOOLKIT_DEBUG("Found existing javascript object for c++ object %p - %s\n", (void*)existing_cpp_object, v8toolkit:: xl::demangle<T>().c_str());
			javascript_object = v8::Local<v8::Object>::New(isolate, this->existing_wrapped_objects[existing_cpp_object]);

		} else {

            V8TOOLKIT_DEBUG("Creating new javascript object for c++ object %p - %s\n", existing_cpp_object, v8toolkit:: xl::demangle<T>().c_str());

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
                v8::TryCatch tc;
				javascript_object = get_function_template()->GetFunction(context).ToLocalChecked()->NewInstance();


				// this shouldn't be able to fire because the FunctionTemplate being used shouldn't have a constructor
				//   call with any content...
				if (tc.HasCaught()) {
					throw V8ExecutionException(this->isolate, tc);
				}

				assert(!javascript_object.IsEmpty());
                // fprintf(stderr, "New object is empty?  %s\n", javascript_object.IsEmpty()?"yes":"no");
                // fprintf(stderr, "Created new JS object to wrap existing C++ object.  Internal field count: %d\n", javascript_object->InternalFieldCount());

                initialize_new_js_object(isolate, javascript_object, existing_cpp_object, destructor_behavior);

                V8TOOLKIT_DEBUG("Wrap existing cpp object returning object about to be cast to a value: %s - %s\n", *v8::String::Utf8Value(javascript_object), v8toolkit:: xl::demangle<T>().c_str());
            }
		}
		return javascript_object;
	}


	// this form is required for selecting between different overloaded instances of the same function name
	// NOTE: If this doesn't match something, make sure it's not a function TEMPLATE instead of a function -- i.e. missing template parameters
	template<class R, class... Params, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name, R(*function)(Params...), DefaultArgs const default_args_tuple = DefaultArgs{}) {
		return add_static_method(method_name, function_type_t<decltype(function)>(function), default_args_tuple);
	};

	static inline std::vector<std::string> reserved_names = {"arguments", "arity", "caller", "displayName",
															 "length", "name", "prototype"};



	template <typename MemberType>
    void add_static_member(const std::string & name,
                           MemberType * member) {

		// must be set before finalization
		assert(!this->finalized);

		if (is_reserved_word_in_static_context(name)) {
			throw InvalidCallException(fmt::format("The name: '{}' is a reserved property in javascript functions, so it cannot be used as a static name", name));
		}
		this->check_if_static_name_used(name);

		auto static_adder = [this, name, member](v8::Local<v8::FunctionTemplate> constructor_function_template) {

			constructor_function_template->
				SetNativeDataProperty(v8::String::NewFromUtf8(isolate, name.c_str()),
									  _variable_getter<MemberType>,
									  _variable_setter<MemberType>,
									  v8::External::New(isolate, member));

		};
		this->static_adders.emplace_back(static_adder);


	}


	/**
	 * Exposes the given function as a method on the JavaScript constructor function.
	 * @param method_name name of the property of the function on the JavaScript constructor function
	 * @param callable function to be exposed to JavaScript
	 * @param default_args_tuple any default arguments to the C++ function `callable` if insufficient arguments are
	 * passed from javascript
	 */
	template<class Callable, class DefaultArgs = std::tuple<>>
	void add_static_method(const std::string & method_name,
						   Callable callable,
						   DefaultArgs const default_args_tuple = DefaultArgs{}) {

		assert(!this->finalized);


		if (is_reserved_word_in_static_context(method_name)) {
			throw InvalidCallException(fmt::format("The name: '{}' is a reserved property in javascript functions, so it cannot be used as a static name", method_name));
		}

		// Does it make sense to add static stuff to the const version?   there's no JavaScript const constructor function to add it to
//		if constexpr(!std::is_const_v<T> && is_wrapped_type_v<std::add_const_t<T>>) {
//			V8ClassWrapper<ConstT>::get_instance(isolate).add_static_method(method_name, callable);
//		}


		this->check_if_static_name_used(method_name);


		auto static_method_adder = [this, method_name, callable, default_args_tuple](v8::Local<v8::FunctionTemplate> constructor_function_template) {

			// the function called is this capturing lambda, which calls the actual function being registered
			auto static_method_function_template =
				v8toolkit::make_function_template(this->isolate, [this, default_args_tuple, callable, method_name](const v8::FunctionCallbackInfo<v8::Value>& info) {

					try {
						function_type_t<Callable> callable_func(callable);
						log.info(LogT::Subjects::WRAPPED_FUNCTION_CALL, "Calling static function {}::{}", xl::demangle<T>(), method_name);
						CallCallable<decltype(callable_func)>()(
							callable_func,
							info,
							get_index_sequence_for_func_function(callable_func),
							default_args_tuple);
					} catch (std::exception & e) {
						log.error(LoggingSubjects::Subjects::RUNTIME_EXCEPTION, "Exception while running static method {}::{}: {}",
								    xl::demangle<T>(), method_name, e.what());
						isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
						return;
					}

				}, method_name);

//		    fprintf(stderr, "Adding static method %s onto %p for %s\n", method_name.c_str(), &constructor_function_template, this->class_name.c_str());
			constructor_function_template->Set(this->isolate,
											   method_name.c_str(),
											   static_method_function_template);
		};

		this->static_adders.emplace_back(static_method_adder);

	}





    /**
    * Function to force API user to declare that all members/methods have been added before any
    *   objects of the wrapped type can be createL10d to make sure everything stays consistent
    * Must be called before adding any constructors or using wrap_existing_object()
    */
	void finalize(bool wrap_as_most_derived = false);

    /**
    * returns whether finalize() has been called on this type for this isolate
    */
	bool is_finalized() {
        return this->finalized;
    }





    template<auto reference_getter, std::enable_if_t<std::is_pointer_v<decltype(reference_getter)>, int> = 0>
    void add_member(std::string_view member_name) {
        assert(this->finalized == false);

        if constexpr(!std::is_const_v<T> && is_wrapped_type_v<std::add_const_t<T>>) {
            V8ClassWrapper<ConstT>::get_instance(isolate).
                template add_member_readonly<reference_getter>(member_name);
        }

        this->check_if_name_used(member_name);

        // store a function for adding the member on to an object template in the future
        member_adders.emplace_back([member_name](v8::Local<v8::ObjectTemplate> & constructor_template){


            constructor_template->SetAccessor(v8::Local<v8::Name>::Cast(make_js_string(member_name)),
                                              _getter_helper<reference_getter>,
                                              _setter_helper<reference_getter>);
        });
    }


    template<auto pimpl_member, auto member>
    void add_member(std::string_view member_name) {
        add_member<+[](T * cpp_object)->auto&{return (*(cpp_object->*pimpl_member)).*member;}>(member_name);
    };


	template<auto member, std::enable_if_t<!std::is_pointer_v<decltype(member)>, int> = 0>
	void add_member(std::string_view member_name) {
	    add_member<+[](T * cpp_object)->auto&{return cpp_object->*member;}>(member_name);
	}


	template<auto reference_getter, std::enable_if_t<std::is_pointer_v<decltype(reference_getter)>, int> = 0>
	void add_member_readonly(std::string_view member_name) {
		assert(this->finalized == false);

		// the field may be added read-only even to a non-const type, so make sure it's added to the const type, too
		if constexpr(!std::is_const_v<T> && is_wrapped_type_v<std::add_const_t<T>>) {
			V8ClassWrapper<ConstT>::get_instance(isolate).template add_member_readonly<reference_getter>(member_name);
		}

		this->check_if_name_used(member_name);

		member_adders.emplace_back([member_name](v8::Local<v8::ObjectTemplate> & constructor_template){

			constructor_template->SetAccessor(make_js_string(member_name),
											  _getter_helper<reference_getter>,
											  0);
		});
	}


	template<auto member, std::enable_if_t<!std::is_pointer_v<decltype(member)>, int> = 0>
	void add_member_readonly(std::string_view member_name) {
		add_member_readonly<+[](T * cpp_object)->auto&{return cpp_object->*member;}>(member_name);
	}

	template<auto pimpl_member, auto member>
	void add_member_readonly(std::string_view member_name) {
		add_member_readonly<+[](T * cpp_object)->auto&{return (*(cpp_object->*pimpl_member)).*member;}>(member_name);
	};



	/**
	 * The specified function will be called when the JavaScript object is called like a function
	 * @param method function to call
	 */
	template<class R, class TBase, class... Args>
	void make_callable(R(TBase::*method)(Args...))
	{
		static_assert(std::is_base_of_v<TBase, T>, "Member function type not from inheritance hierarchy");
	    _add_method("unused name", method, TypeList<Args...>(), std::tuple<>(), true);
	}

    /**
 * The specified function will be called when the JavaScript object is called like a function
 * @param method function to call
 */
    template<class R, class TBase, class... Args>
    void make_callable(R(TBase::*method)(Args...) const)
    {
		static_assert(std::is_base_of_v<TBase, T>, "Member function type not from inheritance hierarchy");
		_add_method("unused name", method, TypeList<Args...>(), std::tuple<>(), true);
    }
    

	/**
	 * rvalue instance functions not supported yet.  Using this triggers a static assertion failure
	 * @param method_name ignored
	 * @param method member ignored
	 * @param default_args ignored
	 */
	template<typename F, typename DefaultArgs = std::tuple<>, typename = std::enable_if_t<xl::is_pointer_to_member_function_v<F>>>
    void add_method(std::string_view method_name, F f, DefaultArgs const & default_args = DefaultArgs())
    {
    	using F_INFO = xl::pointer_to_member_function<F>;
		static_assert(std::is_base_of_v<typename F_INFO::class_type, T>, "Member function class not in inheritance hierarchy");
		static_assert(F_INFO::const_v || !std::is_const_v<T>);
		static_assert(!F_INFO::rvalue_qualified, "not supported");
		
		// volatile `this` can only call volatile member funcs
		static_assert(!std::is_volatile_v<T> || F_INFO::volatile_v);
		
		// add the method to `const T` if appropriate
		if constexpr(!std::is_const_v<T> && is_wrapped_type_v<std::add_const_t<T>> && F_INFO::const_v) {
			V8ClassWrapper<std::add_const_t<T>>::get_instance(isolate)._add_method(method_name, f, typename F_INFO::template type_list<TypeList>(), default_args);
		} 
		
		_add_method(method_name, f, typename F_INFO::template type_list<TypeList>(), default_args);
	}

	
    /**
	* If the method is marked const, add it to the const version of the wrapped type
	*/
	template<class R, class Head, class... Tail, class DefaultArgs = std::tuple<>,
        std::enable_if_t<std::is_const<Head>::value && !std::is_const<T>::value, int> = 0>
	void add_fake_method_for_const_type(std::string_view method_name, func::function<R(Head *, Tail...)> method,
                                        DefaultArgs const & default_args = DefaultArgs()) {
		V8ClassWrapper<ConstT>::get_instance(isolate)._add_fake_method(method_name, method, default_args);
	};


	/**
	 * If the method is not marked const, don't add it to the const type (since it's incompatible)
	 */
	template<class R, class Head, class... Tail, class DefaultArgs = std::tuple<>,
        std::enable_if_t<!(std::is_const<Head>::value && !std::is_const<T>::value), int> = 0>
	void add_fake_method_for_const_type(std::string_view method_name, func::function<R(Head *, Tail...)> method,
                                        DefaultArgs const & default_args = DefaultArgs()) {
		// nothing to do here
	};



	v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object, DestructorBehavior & destructor_behavior) {
		return this->wrap_as_most_derived_object->operator()(cpp_object, destructor_behavior);
	}



	template<class R, class Head, class... Tail, class DefaultArgsTuple,
		std::enable_if_t<std::is_pointer<Head>::value && // Head must be T * or T const *
						 std::is_same<std::remove_const_t<std::remove_pointer_t<Head>>, std::remove_const_t<T>>::value, int> = 0>
	void _add_fake_method(std::string_view method_name, func::function<R(Head, Tail...)> method, DefaultArgsTuple const & default_args)
	{
		assert(this->finalized == false);

		// conditionally add the method to the const type
		add_fake_method_for_const_type(method_name, method);

		this->check_if_name_used(method_name);


		// This puts a function on a list that creates a new v8::FunctionTemplate and maps it to "method_name" on the
		// Object template that will be passed in later when the list is traversed
		fake_method_adders.emplace_back([this, default_args, method_name, method](v8::Local<v8::ObjectTemplate> prototype_template) {

			using CopyFunctionType = func::function<R(Head, Tail...)>;
			using FakeType = std::conditional_t<std::is_const_v<T>, Head, std::remove_const_t<Head>>;
			CopyFunctionType * copy = new func::function<R(FakeType, Tail...)>(method);


			// This is the actual code associated with "method_name" and called when javascript calls the method
			StdFunctionCallbackType * method_caller =
					new StdFunctionCallbackType([method_name, default_args, copy](const v8::FunctionCallbackInfo<v8::Value>& info) {


				auto fake_method = *(func::function<R(Head, Tail...)>*)v8::External::Cast(*(info.Data()))->Value();
				auto isolate = info.GetIsolate();

				// auto holder = info.Holder();

                // it should not be 0, and right now there is no known way for it being more than 1.
                //assert(holder->InternalFieldCount() == 1);

                // a crash here may have something to do with a native override of toString

                auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(info.Holder());
                log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "fake method {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);


				// V8 does not support C++ exceptions, so all exceptions must be caught before control
				//   is returned to V8 or the program will instantly terminate
				try {
                    //
				    CallCallable<CopyFunctionType, FakeType>().template operator()(*copy, info, cpp_object, std::make_integer_sequence<int, sizeof...(Tail)>(), default_args); // just Tail..., not Head, Tail...
				} catch(std::exception & e) {
					log.error(LoggingSubjects::Subjects::RUNTIME_EXCEPTION, "Exception while running 'fake method' {}::{}: {}",
							    xl::demangle<T>(), method_name, e.what());

					isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));
					return;
				}
				return;
			});

			// create a function template, set the lambda created above to be the handler
			auto function_template = v8::FunctionTemplate::New(this->isolate);
			function_template->SetCallHandler(callback_helper, v8::External::New(this->isolate, method_caller));

			// methods are put into the protype of the newly created javascript object
			prototype_template->Set(make_js_string(method_name), function_template);
		});
	}


	/**
	 * Creates a "fake" method from any callable which takes a T* as its first parameter.  Does not create
	 *   the method on the const type
	 * @param method_name JavaScript name to expose this method as
	 * @param method method to call when JavaScript function invoked
	 */
	template<typename F, typename DefaultArgs = std::tuple<>,
		typename = decltype(
		    static_cast<void(V8ClassWrapper::*)(
		        std::string_view,
                decltype(func::function(std::declval<F>())),
                DefaultArgs const &
            )>(&V8ClassWrapper::_add_fake_method)
		    )>
	void add_method(std::string_view method_name, F && f,
					DefaultArgs const & default_args = DefaultArgs()) {
		_add_fake_method(method_name, func::function(std::forward<F>(f)), default_args);
	}



	v8::Local<v8::Object> get_matching_prototype_object(v8::Local<v8::Object> object) {
		// the object in the prototype chain that was constructed by a FunctionTemplate associated
		//   with this V8ClassWrapper instance
		v8::Local<v8::Object> result;

		for (auto & function_template : this->this_class_function_templates) {
			result = object->FindInstanceInPrototypeChain(function_template.Get(isolate));
			if (!result.IsEmpty() && !result->IsNull()) {
				break;
			}
		}
		if (result.IsEmpty()) {
			throw InvalidCallException(
				fmt::format(
					"Object did not have an object of type {} in its prototype chain",
					 xl::demangle<T>()));
		}
		return result;
	}



    template<class M, class... Args, class... DefaultArgTypes>
	void _add_method(std::string_view method_name,
                                    M method,
                                    TypeList<Args...> const &,
                                    std::tuple<DefaultArgTypes...> const & default_args_tuple,
                                    bool add_as_callable_object_callback = false) {
		assert(this->finalized == false);

		this->check_if_name_used(method_name);


		// adds this method to the list of methods to be added when a new "javascript constructor"
		// is created for the type being wrapped
		MethodAdderData method_adder_data =
			MethodAdderData{std::string(method_name), StdFunctionCallbackType(

				// This is the function called (via a level of indirection because this is a capturing lambda)
				//   when the JavaScript method is called.
				[this, default_args_tuple, method, method_name]
					(const v8::FunctionCallbackInfo<v8::Value> & info) {
					auto isolate = info.GetIsolate();

					auto self = get_matching_prototype_object(info.Holder());


					auto cpp_object = get_cpp_object(self);
					log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "method '{}' got {} pointer: {}", method_name, xl::demangle<T>(), (void*)cpp_object);
					
					if (cpp_object == nullptr) {
						auto stack_trace = get_stack_trace_string(v8::StackTrace::CurrentStackTrace(v8::Isolate::GetCurrent(), 100));
			
						std::cerr << fmt::format("{}\n", stack_trace);

						std::cerr << fmt::format("PROBLEM HERE\n");
					}

					auto bound_method = func::function([&cpp_object, method](Args... args)->decltype(auto){
                        return (cpp_object->*method)(std::forward<Args>(args)...);
					});

					// V8 does not support C++ exceptions, so all exceptions must be caught before control
					//   is returned to V8 or the program will instantly terminate
					try {
						log.info(LogT::Subjects::WRAPPED_FUNCTION_CALL, "Calling instance member function {}::{}", xl::demangle<T>(), method_name);
						// make a copy of default_args_tuple so it's non-const - probably better to do this on a per-parameter basis
						CallCallable<decltype(bound_method)>()(bound_method, info,
															   std::make_integer_sequence<int, sizeof...(Args)>{},
															   std::tuple<DefaultArgTypes...>(
																   default_args_tuple));
					} catch (std::exception & e) {
						log.error(LoggingSubjects::Subjects::RUNTIME_EXCEPTION, "Exception while running method {}::{}: {}",
								    xl::demangle<T>(), method_name, e.what());

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


	using EnumValueMap = MapT<std::string, double>;
	struct EnumData {
		EnumValueMap enum_value_map;

		// the frozen javascript object containing the enum property names and values
		// lazily created because no context exists to create it in when add_enum is called
		v8::Global<v8::Object> js_enum_object;

		EnumData(EnumValueMap && enum_value_map) : enum_value_map(std::move(enum_value_map)) {}
	};

	void add_enum(xl::zstring_view const & name, EnumValueMap && value_map) {

		auto enum_data = new EnumData(std::move(value_map));

		this->enum_adders.emplace_back([this, name, enum_data](v8::Local<v8::ObjectTemplate> object_template) {

			// Accessor callback
			object_template->SetAccessor(
				v8::String::NewFromUtf8(this->isolate, name.c_str()), [](v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info){
					auto isolate = info.GetIsolate();
					// get EnumData object from info.Data()'s v8::External
					auto external = v8::Local<v8::External>::Cast(info.Data());
					auto enum_data = static_cast<EnumData *>(external->Value());

					// check to see if a JS object has been created
					if (enum_data->js_enum_object.IsEmpty()) {
//						std::cerr << fmt::format("Creating one-time enum object") << std::endl;
						auto object = v8::Object::New(isolate);
						enum_data->js_enum_object.Reset(isolate, object);

						// initialize object with actual properties and values from enum
						for (auto const & pair : enum_data->enum_value_map) {
							object->Set(
								v8::String::NewFromUtf8(isolate, pair.first.c_str()),
								v8::Number::New(isolate, pair.second)
							);
						}

						// freeze the JS object so it can never be changed later
						//object->SetIntegrityLevel(isolate->GetCurrentContext(), v8::IntegrityLevel::kFrozen);

					} else {
//						std::cerr << fmt::format("using existing enum object") << std::endl;
					}


					// return the JS object
					info.GetReturnValue().Set(enum_data->js_enum_object.Get(isolate));
				}, nullptr, v8::External::New(this->isolate, enum_data));
		});
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
		if constexpr(!std::is_const_v<T>) {
			V8ClassWrapper<ConstT>::get_instance(this->isolate).add_new_constructor_function_template_callback(callback);
		}

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
    MapT<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_TEMPLATE_SFINAE>::isolate_to_wrapper_map;

template<class T>
class JSWrapper;




/**
 * If Getting strange errors about needing type definitions for class member destructors, make sure
 * all constructors and destructors are =default'd/defined in .cpp files, not headers if the types are
 * desired to be forward declared
 * @tparam T 
 * @tparam Behavior 
 */
template<class T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<is_wrapped_type_v<T>>> {

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


template<class T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
	xl::is_template_for_v<std::unique_ptr, T> &&
	is_wrapped_type_v<typename std::remove_reference_t<T>::element_type>
>>
{
	using NoRefT = std::remove_reference_t<T>;

	v8::Local<v8::Value> operator()(v8::Isolate * isolate, NoRefT & unique_ptr) {

		if (unique_ptr.get() == nullptr) {
			return v8::Undefined(isolate);
		} else {
			return Behavior()(unique_ptr.get());
		}
	}

	v8::Local<v8::Value> operator()(v8::Isolate *isolate, NoRefT && unique_ptr) {

		if (unique_ptr.get() == nullptr) {
			return v8::Undefined(isolate);
		}

		auto & wrapper = V8ClassWrapper<typename NoRefT::element_type>::get_instance(isolate);

		// create new owning JavaScript object with the contents of the unique_ptr
		auto result = wrapper.wrap_existing_cpp_object(isolate->GetCurrentContext(), unique_ptr.get(), *wrapper.destructor_behavior_delete);
		(void)unique_ptr.release();
		return result;

	}
};


template<class T, typename Behavior>
struct CastToJS<T, Behavior, std::enable_if_t<
	std::is_pointer_v<std::remove_cv_t<std::remove_reference_t<T>>> && // must be a pointer
	!std::is_pointer_v<std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<T>>>> && // no multi-pointers
	is_wrapped_type_v<std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<T>>>>
>> {

	// this may still be const
	using WrappedType = std::remove_pointer_t<std::remove_cv_t<std::remove_reference_t<T>>>;

    // An lvalue is presented, so the memory will not be cleaned up by JavaScript
    v8::Local<v8::Value> operator()(v8::Isolate * isolate, T cpp_object) {
        if (cpp_object == nullptr) {
            return v8::Undefined(isolate);
        }
		
        V8TOOLKIT_DEBUG("CastToJS from T %s\n",  xl::demangle<_typeid_name(typeid(T).name()).c_str());
        auto context = isolate->GetCurrentContext();
        V8ClassWrapper<WrappedType> & class_wrapper = V8ClassWrapper<WrappedType>::get_instance(isolate);

#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
//		std::cerr << fmt::format("Checking if CastToJS type is a JSWrapper<(remove_const of){}>", xl::demangle<WrappedType>()) << std::endl;

        using JSWrapperType = JSWrapper<WrappedType>;
//		fprintf(stderr, "Checking to see if object * is a JSWrapper *\n");

        if constexpr(std::is_const<WrappedType>::value) {
            auto js_wrapper = safe_dynamic_cast<JSWrapperType const *>(cpp_object);
            if (js_wrapper) {
//            	std::cerr << fmt::format("** YES IS A JSWRAPPER") << std::endl;
                return Behavior()(*js_wrapper);
            }
        } else {

            auto js_wrapper = safe_dynamic_cast<xl::match_const_of_t<JSWrapperType, WrappedType> *>(cpp_object);
            if (js_wrapper) {
//				std::cerr << fmt::format("** YES IS A JSWRAPPER") << std::endl;
                return Behavior()(*js_wrapper);
            }
        }
//		std::cerr << fmt::format("** NO IS NOT A JSWRAPPER") << std::endl;

#else
//        std::cerr << fmt::format("Not checking if CastToJS type is a JSWrapper") << std::endl;

#endif
        V8TOOLKIT_DEBUG("CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

        /** If you are here looking for an INFINITE RECURSION make sure the type is wrapped **/
        return class_wrapper.template wrap_existing_cpp_object(context, cpp_object,
                                                               *class_wrapper.destructor_behavior_leave_alone);
    }
};


template<class T, typename Behavior>
struct CastToJS<T&, Behavior, std::enable_if_t<is_wrapped_type_v<T>>> {

	// An lvalue is presented, so the memory will not be cleaned up by JavaScript
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object) {
		return Behavior()(&cpp_object);
	}
};


template<class T, typename Behavior>
struct CastToJS<T&&, Behavior, std::enable_if_t<is_wrapped_type_v<T>>> {

	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object) {
		return Behavior()(std::make_unique<T>(std::move(cpp_object)));
	}
};


template<typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<
    !std::is_reference_v<T> &&
	std::is_copy_constructible<T>::value && 
	is_wrapped_type_v<T>>>
{
	T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		auto pointer_result = CastToNative<T*>()(isolate, value);
		if (pointer_result == nullptr) {
			throw CastException("Javascript Value could not be converted to {} to use for copy constructor", xl::demangle<T>());
		}
		return T(*pointer_result);
	}
	static constexpr bool callable(){return true;}
};


template<typename T, typename Behavior>
struct CastToNative<T, Behavior, std::enable_if_t<
	!std::is_reference_v<T> && 
	!std::is_copy_constructible_v<T> && 
    std::is_move_constructible_v<T> &&
    is_wrapped_type_v<std::remove_reference_t<T>>
>>
{
	template<class U = T> // just to make it dependent so the static_asserts don't fire before `callable` can be called
	T operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {
        if (V8ClassWrapper<T>::does_object_own_memory(get_value_as<v8::Object>(value))) {
            auto && result = Behavior().template operator()<T&&>(value);
            return T(std::move(result));
        } else {
            throw CastException("Could not move construct object of type {} from JavaScript object which does not own its memory", xl::demangle<T>());
        }
	}
	static constexpr bool callable(){return false;}

};


template<typename T, typename Behavior>
struct CastToNative<T&, Behavior, std::enable_if_t<is_wrapped_type_v<T>>>
{
	T& operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		auto pointer_result = Behavior().template operator()<T*>(value);
		assert(pointer_result != nullptr);
		return *pointer_result;
	}
	static constexpr bool callable(){return true;}
};


template<typename T, typename Behavior>
struct CastToNative<T&&, Behavior, std::enable_if_t<is_wrapped_type_v<T>>>
{
	// to "give permission" to have the object moved out of, this object must own the memory.  It CANNOT
	//   release ownership of the memory for the object, just the data in the object
	// TODO: Is this the right model?  This seems the safest policy, but is technically restricting of potentially valid operations
	T&& operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
		v8::Local<v8::Object> object = get_value_as<v8::Object>(isolate, value);
		T * cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(object);
		log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "CastToNative T&& got {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);

		if (V8ClassWrapper<T>::does_object_own_memory(object)) {
			// do not release the memory because something still needs to call delete on the object (possibly moved out of)
			//   pointed by the InternalField of the javascript object -- even if it has no "real" content.
			return std::move(*cpp_object);
		}
		throw CastException("Could not cast object to {} && because it doesn't own it's memory",  xl::demangle<T>());
	}
	static constexpr bool callable(){return true;}
};


template<typename T, typename Behavior>
struct CastToNative<T*, Behavior, std::enable_if_t<is_wrapped_type_v<T>>>
{
	T* operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) {
	    if (value->IsNullOrUndefined()) {
			throw CastException("Cannot cast null/undefined object to {}*", xl::demangle<T>());
		}
		auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
		v8::Local<v8::Object> object = get_value_as<v8::Object>(isolate, value);

		auto cpp_object = wrapper.get_cpp_object(object);
		log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "CastToNative T* got {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);

		return cpp_object;
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
     throw CastException(fmt::format("Pointer and reference types ({}) won't ever succeed in getting an embedded cpp object",  xl::demangle<T>()));
 }

/**
 * This can be used from CastToNative<UserType> calls to fall back to if other conversions aren't appropriate
 */
template<class T, std::enable_if_t<!std::is_pointer<T>::value && !std::is_reference<T>::value, int> = 0>
T & get_object_from_embedded_cpp_object(v8::Isolate * isolate, v8::Local<v8::Value> value) {

	V8TOOLKIT_DEBUG("cast to native\n");

	auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).get_cpp_object(get_value_as<v8::Object>(isolate, value));
	log.info(LogT::Subjects::WRAPPED_DATA_MEMBER_ACCESS, "get_object_from_embedded_cpp_object got {} pointer: {}", xl::demangle<T>(), (void*)cpp_object);

//	 std::cerr << fmt::format("about to call cast on {}",  xl::demangle<T>()) << std::endl;
	if (cpp_object == nullptr) {
		fprintf(stderr, "Failed to convert types: want:  %d %s\n", std::is_const<T>::value, typeid(T).name());
		throw CastException(fmt::format("Cannot convert AnyBase to {}",  xl::demangle<T>()));
	}
//		std::cerr << fmt::format("Successfully converted") << std::endl;
	return *cpp_object;
}

template<class T, class... Rest, typename Behavior>
struct CastToNative<std::unique_ptr<T, Rest...>, Behavior, std::enable_if_t<is_wrapped_type_v<T>>>
{
	std::unique_ptr<T, Rest...> operator()(v8::Isolate * isolate, v8::Local<v8::Value> value) const {

		auto object = get_value_as<v8::Object>(isolate, value);

		// if the javascript object owns the memory, it gives up that ownership to the unique_ptr
		if (V8ClassWrapper<T>::does_object_own_memory(object)) {
			auto & wrapper = V8ClassWrapper<T>::get_instance(isolate);
			return std::unique_ptr<T, Rest...>(wrapper.release_internal_field_memory(object));
		} else if constexpr(std::is_copy_constructible_v<T>) {
			T & cpp_object = get_object_from_embedded_cpp_object<T>(isolate, value);
			return std::unique_ptr<T, Rest...>(new T(cpp_object));
		} else {
			throw CastException("Cannot CastToNative<unique_ptr<{}>> from object that doesn't own it's memory and isn't copy constructible: {}",
				 xl::demangle<T>(), *v8::String::Utf8Value(value));
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
//	ANYBASE_PRINT("typechecker::check for {}  with anyptr {} (string: {})",  xl::demangle<Head>(), (void*)any_base, any_base->type_name);
	if(auto any = dynamic_cast<AnyPtr<Head> *>(any_base)) {
//		ANYBASE_PRINT("Got match on: {}, returning {}",  xl::demangle<Head>(), (void*)(any->get()));
		return static_cast<T*>(any->get());
	}

//	ANYBASE_PRINT("didn't find match, testing const type now...");

	// if goal type is const and the type to check isn't const, try checking for the const type now
	if constexpr(!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
        if (auto derived_result = V8ClassWrapper<Head>::get_instance(this->isolate).cast(any_base)) {
            return derived_result;
        }
	}

//	ANYBASE_PRINT("no match on const type either, continuing down chain");

	return SUPER::check(any_base, false);
}




// if a more-derived type was found, pass it to that type to see if there's something even more derived
template<class T, class Head, class... Tail>
v8::Local<v8::Object> WrapAsMostDerived<T, v8toolkit::TypeList<Head, Tail...>,
	std::enable_if_t<!std::is_const<T>::value || std::is_const<Head>::value>>
::operator()(T * cpp_object, DestructorBehavior & destructor_behavior) const {

	// if they're the same, let it fall through to the empty typechecker TypeList base case
	if constexpr(!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
		using MatchingConstT = std::conditional_t<std::is_const<Head>::value, std::add_const_t<T>, std::remove_const_t<T>>;

		if constexpr(std::is_const<T>::value == std::is_const<Head>::value) {
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
	T /* T& or T&& */ operator()(const v8::FunctionCallbackInfo<v8::Value> & info, int & i,
					std::vector<std::unique_ptr<StuffBase>> & stuff,
					DefaultArgsTuple && default_args_tuple = DefaultArgsTuple()) {
		PB_PRINT("ParameterBuilder handling wrapped type: {} {}",  xl::demangle<T>(),
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
            if constexpr(constructed_from_callback_info_v<NoConstRefT>) {
                stuff.emplace_back(std::make_unique<Stuff<NoConstRefT>>(NoConstRefT(info)));
                return std::forward<T>(*(static_cast<Stuff<NoConstRefT> &>(*stuff.back()).get()));

            }
			if constexpr(std::is_move_constructible_v<NoConstRefT> && CastToNative<NoConstRefT>::callable())
			{
				auto result = CastToNative<NoConstRefT>()(isolate, value);
				stuff.emplace_back(std::make_unique<Stuff<NoConstRefT>>(std::move(result)));
				return std::forward<T>(*(static_cast<Stuff<NoConstRefT> &>(*stuff.back()).get()));
			}
			throw CastException("Could not create requested object of type: {} {}.  Maybe you don't 'own' your memory?",
								 xl::demangle<T>(),
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
		PB_PRINT("ParameterBuilder handling wrapped type: {}",  xl::demangle<T>());
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


} // end namespace v8toolkit

