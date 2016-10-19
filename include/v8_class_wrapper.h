#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>
#include <algorithm>

#include <functional>
#include <iostream>
#include <map>
#include <vector>
#include <utility>
#include <assert.h>

#include "wrapped_class_base.h"
#include "v8toolkit.h"


#include "casts.hpp"


#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
#define V8_CLASS_WRAPPER_HAS_BIDIRECTIONAL_SUPPORT
#endif

namespace v8toolkit {


#define V8_CLASS_WRAPPER_DEBUG false

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
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Deleting object at %p during V8 garbage collection\n", void_object);
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
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Not deleting object %p during V8 garbage collection\n", void_object);
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

    // returns cpp_object wrapped as its most derived type
	virtual v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) const = 0;

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
#ifdef ANYBASE_DEBUG
        std::cerr << fmt::format("Failed to find match for anybase with type string: {}", any_base->type_name)
                  << std::endl;
#endif
        return nullptr;
    }

    virtual v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) const override;
};


// Specialization for types that cannot possibly work -- casting const value to non-const return
template<class T, class Head, class... Tail>
struct TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,
    // if the type we want isn't const but the type being checked is, it cannot be a match
    std::enable_if_t<!std::is_const<T>::value && std::is_const<Head>::value>> : public TypeChecker<T, TypeList<Tail...>> {

    using SUPER = TypeChecker<T, TypeList<Tail...>>;

	TypeChecker(v8::Isolate * isolate) : SUPER(isolate) {}


    virtual T * check(AnyBase * any_base, bool first_call = true) const override {

#ifdef ANYBASE_DEBUG
        //	std::cerr << fmt::format("In Type Checker<{}> SKIPPING CHECKING if it is a (const) {}", demangle<T>(), demangle<Head>()) << std::endl;
        if (dynamic_cast<AnyPtr<Head> *>(any_base) != nullptr) {
            std::cerr
                << "ERROR:::: But if I would have checked, it would have been a match!  Should you be casting to a const type instead?"
                << std::endl;
            assert(false);
        }
#endif

        return SUPER::check(any_base);
    }

	virtual v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) const override {
		return SUPER::wrap_as_most_derived(cpp_object);
	}

};

 
// tests an AnyBase * against a list of types compatible with T
//   to see if the AnyBase is an Any<TypeList...> ihn
template<class T, class Head, class... Tail>
    struct TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,

    // if it's *not* the condition of the specialization above
    std::enable_if_t<!(!std::is_const<T>::value && std::is_const<Head>::value)>
                     > : public TypeChecker<T, TypeList<Tail...>> {
    
   
    using SUPER = TypeChecker<T, TypeList<Tail...>>;
	TypeChecker(v8::Isolate * isolate) : SUPER(isolate) {}

	virtual T * check(AnyBase * any_base, bool first_call = true) const override;

	virtual v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) const override;
};





 // Cannot make class wrappers for pointer or reference types
#define V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE !std::is_pointer<T>::value && !std::is_reference<T>::value

#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX std::is_base_of<WrappedClassBase, T>::value
/**
 * Allows user to specify a list of types to instantiate real V8ClassWrapper template for -- CastToNative/CastToJS will otherwise
 *   try to instantiate it for a very large number of types which can drastically slow down compilation.
 * Setting an explicit value for this is NOT required - it is just a compile-time, compile-RAM (and maybe binary size) optimizatioan
 */
#ifndef V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE
 class UnusedType;
#define V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE !std::is_same<UnusedType, T>::value
#endif


 // uncomment this to see the effects of generating the wrapper class on compile time (but won't actually run correctly)
 //#define TEST_NO_REAL_WRAPPERS
 

 #ifdef TEST_NO_REAL_WRAPPERS
 class UnusedType;
#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<std::is_same<T, UnusedType>::value>
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<!std::is_same<T, UnusedType>::value>

 #else
// Use the real V8ClassWrapper specialization if the class inherits from WrappedClassBase or is in the user-provided sfinae
#define V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX || (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE))>

// otherwise use the 'cheap' specialization
#define V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE std::enable_if_t<(V8TOOLKIT_V8CLASSWRAPPER_NO_POINTER_NO_REFERENCE_SFINAE) && \
    !(V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE_PREFIX || (V8TOOLKIT_V8CLASSWRAPPER_FULL_TEMPLATE_SFINAE))>
#endif



	
 
/**
 * The real template is quite expensive to make for types that don't need it, so here's an alternative for when it isn't actually going to be used
 */
 template<class T, class = void> class V8ClassWrapper;

 template<class T>
     class V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_FAKE_TEMPLATE_SFINAE> {
 public:
     static V8ClassWrapper<T> & get_instance(v8::Isolate * isolate){throw std::exception();}
     template<class BEHAVIOR>
     v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object, bool force_wrap_this_type = false) {throw std::exception();}

     T * cast(AnyBase * any_base){
	 throw std::exception();
     }


      	template<class R, class TBase, class... Args,
			 std::enable_if_t<std::is_same<TBase,T>::value || std::is_base_of<TBase, T>::value, int> = 0>
	V8ClassWrapper<T> & add_method(const std::string & method_name, R(TBase::*method)(Args...) const) {
	    throw std::exception();
	}

	template<class R, class TBase, class... Args,
	    std::enable_if_t<std::is_same<TBase,T>::value || std::is_base_of<TBase, T>::value, int> = 0>
	    V8ClassWrapper<T> & add_method(const std::string & method_name, R(TBase::*method)(Args...)) {
	    throw std::exception();
	}
	template<class R, class... Args>
	    void add_method(const std::string & method_name, std::function<R(T*, Args...)> & method) {
	    throw std::exception();
	}


	/**
	 * Takes a lambda taking a T* as its first parameter and creates a 'fake method' with it
	 */
	template<class Callback>
	V8ClassWrapper<T> & add_method(const std::string & method_name, Callback && callback) {
	    throw std::exception();
	}

		    

 	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
 v8toolkit::V8ClassWrapper<T>& add_constructor(std::string js_constructor_name, v8::Local<v8::ObjectTemplate> parent_template)
 {throw std::exception();}

 void finalize(bool wrap_as_most_derived = false){throw std::exception();}

     template<class MEMBER_TYPE, class MemberClass>
	V8ClassWrapper<T> & add_member(std::string member_name, MEMBER_TYPE MemberClass::* member, bool = false)
 {throw std::exception();}

    template<class... CompatibleTypes>
    std::enable_if_t<static_all_of<std::is_base_of<T,CompatibleTypes>::value...>::value, V8ClassWrapper<T>&>
 set_compatible_types(){throw std::exception();}

     template<class ParentType>
    std::enable_if_t<std::is_base_of<ParentType, T>::value, V8ClassWrapper<T>&>
 set_parent_type(){throw std::exception();}
 
     
     template<class R, class... Params>
	 V8ClassWrapper<T> & add_static_method(const std::string & method_name, R(*callable)(Params...)) {
	 throw std::exception();
     }

     template<class Callable>
	 V8ClassWrapper<T> & add_static_method(const std::string & method_name, Callable callable) {
	 throw std::exception();
     }

     T * get_cpp_object(v8::Local<v8::Object> object);

     
 V8ClassWrapper<T> & set_class_name(const std::string & name){throw std::exception();}

     template<class MEMBER_TYPE, class MemberClass>
 V8ClassWrapper<T> & add_member_readonly(std::string member_name, MEMBER_TYPE MemberClass::* member, bool = false){throw std::exception();}

 v8::Local<v8::FunctionTemplate> get_function_template(){throw std::exception();}

 template<class DestructorBehavior>
 static void initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object){throw std::exception();}


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
 public:


    
private:


	/**
	 * Wrapped classes are per-isolate, so this tracks each wrapped class/isolate tuple for later retrieval
	 */
	static std::map<v8::Isolate *, V8ClassWrapper<T> *> isolate_to_wrapper_map;


	using AttributeChangeCallback = std::function<void(v8::Isolate * isolate,
													   v8::Local<v8::Object> & self,
													   const std::string &,
													   const v8::Local<v8::Value> & value)>;

	/// List of callbacks for when attributes change
	std::vector<AttributeChangeCallback> attribute_callbacks;

	V8ClassWrapper() = delete;
	V8ClassWrapper(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper(const V8ClassWrapper<T> &&) = delete;
	V8ClassWrapper& operator=(const V8ClassWrapper<T> &) = delete;
	V8ClassWrapper& operator=(const V8ClassWrapper<T> &&) = delete;
	
	
	/**
	 * users of the library should call get_instance, not this constructor directly
	 */
	V8ClassWrapper(v8::Isolate * isolate);

	/**
	 * List of names already in use for methods/static methods/accessors
	 * Used to make sure duplicate names aren't requested
	 */
    std::vector<std::string> used_attribute_name_list;
    std::vector<std::string> used_static_attribute_name_list;


	void call_callbacks(v8::Local<v8::Object> object, const std::string & property_name, v8::Local<v8::Value> & value);

	/**
	 * Throws if name has already been checked by this function for this type in this isolate
	 */
    void check_if_name_used(const std::string & name);

    /**
     * static methods go on the constructor function, so it can have names which overlap with the per-instance object attributes
     * @param name name of static method to check
     */
    void check_if_static_name_used(const std::string & name);

    // function used to return the value of a C++ variable backing a javascript variable visible
    //   via the V8 SetAccessor method
	template<class VALUE_T> // type being returned
	static void _getter_helper(v8::Local<v8::String> property,
	                  const v8::PropertyCallbackInfo<v8::Value>& info) {

		auto isolate = info.GetIsolate();
		v8::Local<v8::Object> self = info.Holder();				   
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
		WrappedData<T> * wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
		auto cpp_object = V8ClassWrapper<T>::get_instance(isolate).cast(static_cast<AnyBase *>(wrapped_data->native_object));
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Getter helper got cpp object: %p\n", cpp_object);
		// This function returns a reference to member in question
		auto attribute_data_getter = (AttributeHelperDataCreator<VALUE_T> *)v8::External::Cast(*(info.Data()))->Value();
		auto attribute_data = (*attribute_data_getter)(cpp_object);
		info.GetReturnValue().Set(CastToJS<VALUE_T>()(isolate, attribute_data.member_reference));
	}

    // function used to set the value of a C++ variable backing a javascript variable visible
    //   via the V8 SetAccessor method
	template<typename VALUE_T, std::enable_if_t<std::is_copy_assignable<VALUE_T>::value, int> = 0>
	static void _setter_helper(v8::Local<v8::String> property, v8::Local<v8::Value> value,
	               const v8::PropertyCallbackInfo<void>& info) {

	    auto isolate = info.GetIsolate();
	    v8::Local<v8::Object> self = info.Holder();		   
	    v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));
	    WrappedData<T> * wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
	    T * cpp_object = V8ClassWrapper<T>::get_instance(isolate).cast(static_cast<AnyBase *>(wrapped_data->native_object));
	    
	    auto attribute_data_getter = (AttributeHelperDataCreator<VALUE_T> *)v8::External::Cast(*(info.Data()))->Value();
	    auto attribute_data = (*attribute_data_getter)(cpp_object);

	    // assign the new value to the c++ class data member
	    attribute_data.member_reference = CastToNative<typename std::remove_reference<VALUE_T>::type>()(isolate, value);

	    // call any registered change callbacks
	    attribute_data.class_wrapper.call_callbacks(self, *v8::String::Utf8Value(property), value);

	}
	


	template<typename VALUE_T, std::enable_if_t<!std::is_copy_assignable<VALUE_T>::value, int> = 0>
	static void _setter_helper(v8::Local<v8::String> property, v8::Local<v8::Value> value,
							   const v8::PropertyCallbackInfo<void>& info)
        {}





	// Helper for creating objects when "new MyClass" is called from javascript
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	static void v8_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
		auto isolate = args.GetIsolate();
        
		T * new_cpp_object = nullptr;
		std::function<void(CONSTRUCTOR_PARAMETER_TYPES...)> constructor =
				[&new_cpp_object](CONSTRUCTOR_PARAMETER_TYPES... args)->void{new_cpp_object = new T(std::forward<CONSTRUCTOR_PARAMETER_TYPES>(args)...);};
        
		CallCallable<decltype(constructor)>()(constructor, args);
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "In v8_constructor and created new cpp object at %p\n", new_cpp_object);

		// if the object was created by calling new in javascript, it should be deleted when the garbage collector 
		//   GC's the javascript object, there should be no c++ references to it
		initialize_new_js_object<DestructorBehavior_Delete<T>>(isolate, args.This(), new_cpp_object);
		
		// // return the object to the javascript caller
		args.GetReturnValue().Set(args.This());
	}
	
	// takes a Data() parameter of a StdFunctionCallbackType lambda and calls it
	//   Useful because capturing lambdas don't have a traditional function pointer type
	static void callback_helper(const v8::FunctionCallbackInfo<v8::Value>& args);


	std::map<T *, v8::Global<v8::Object>> existing_wrapped_objects;
	v8::Isolate * isolate;

	// Stores a functor capable of converting compatible types into a <T> object
	std::unique_ptr<TypeCheckerBase<T>> type_checker = std::make_unique<TypeChecker<T, TypeList<std::add_const_t<T>, std::remove_const_t<T>>>>(this->isolate);
        
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

public:
    void register_callback(AttributeChangeCallback callback);
    

	// Common tasks to do for any new js object regardless of how it is created
	template<class DestructorBehavior>
	static void initialize_new_js_object(v8::Isolate * isolate, v8::Local<v8::Object> js_object, T * cpp_object)
	{
//        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Initializing new js object for %s for v8::object at %p and cpp object at %p\n", typeid(T).name(), *js_object, cpp_object);
	    WrappedData<T> * wrapped_data = new WrappedData<T>(new AnyPtr<T>(cpp_object));

//        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "inserting anyptr<%s>at address %p pointing to cpp object at %p\n", typeid(T).name(), any, cpp_object);
		assert(js_object->InternalFieldCount() >= 1);
	    js_object->SetInternalField(0, v8::External::New(isolate, wrapped_data));
		
		// tell V8 about the memory we allocated so it knows when to do garbage collection
		isolate->AdjustAmountOfExternalAllocatedMemory(sizeof(T));
		
		v8toolkit::global_set_weak(isolate, js_object, [isolate, cpp_object]() {
				DestructorBehavior()(isolate, cpp_object);
			}
		);


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

    
    T * get_cpp_object(v8::Local<v8::Object> object);

	
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
        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "isolate to wrapper map %p size: %d\n", &isolate_to_wrapper_map, (int)isolate_to_wrapper_map.size());

        if (isolate_to_wrapper_map.find(isolate) == isolate_to_wrapper_map.end()) {
            auto new_object = new V8ClassWrapper<T>(isolate);
            if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Creating instance %p for isolate: %p\n", new_object, isolate);
        }
        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "(after) isolate to wrapper map size: %d\n", (int)isolate_to_wrapper_map.size());

        auto object = isolate_to_wrapper_map[isolate];
        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Returning v8 wrapper: %p\n", object);
        return *object;

    }


    /**
     * Specify the name of the object which will be used for debugging statements as well as 
     *   being the type returned from javascript typeof
     */
    V8ClassWrapper<T> & set_class_name(const std::string & name);

    

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
    std::enable_if_t<static_all_of<std::is_base_of<T,CompatibleTypes>::value...>::value, V8ClassWrapper<T>&>
    set_compatible_types()
    {
        assert(!is_finalized());
	
	if (!std::is_const<T>::value) {
	    using ConstT = std::add_const_t<T>;
	    V8ClassWrapper<ConstT>::get_instance(isolate).template set_compatible_types<std::add_const_t<CompatibleTypes>...>();
	}

	// Try to convert to T any of:  T, non-const T, any explicit compatible types and their const versions
	type_checker.reset(new TypeChecker<T, TypeList<std::add_const_t<T>, std::remove_const_t<T>, CompatibleTypes..., std::add_const_t<CompatibleTypes>...>>(this->isolate));

        return *this;
    }
	
	
    /**
     * This wrapped class will inherit all the methods from the parent type (and its parent...)
     *
     * It is VERY important that the type being marked as the parent type has this type set with
     *   set_compatible_types<>()
     */
    template<class ParentType>
    std::enable_if_t<std::is_base_of<ParentType, T>::value, V8ClassWrapper<T>&>
    set_parent_type()
    {
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
	    v8::Global<v8::FunctionTemplate>(isolate, V8ClassWrapper<ParentType>::get_instance(isolate).get_function_template());
        return *this;
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
	template<typename ... CONSTRUCTOR_PARAMETER_TYPES>
	v8toolkit::V8ClassWrapper<T>& add_constructor(const std::string & js_constructor_name, v8::Local<v8::ObjectTemplate> parent_template)
	{				
	    assert(((void)"Type must be finalized before calling add_constructor", this->finalized) == true);
	    
	    auto constructor_template =
		make_wrapping_function_template(&V8ClassWrapper<T>::template v8_constructor<CONSTRUCTOR_PARAMETER_TYPES...>,
						v8::Local<v8::Value>());
	    
	    // Add the constructor function to the parent object template (often the global template)
//	    std::cerr << "Adding constructor to global with name: " << js_constructor_name << std::endl;
	    parent_template->Set(v8::String::NewFromUtf8(isolate, js_constructor_name.c_str()), constructor_template);
	    
	    return *this;
	}


	/**
	 * When you don't want a "constructor" but you still need something to attach the static method names to, use this
	 */
	v8toolkit::V8ClassWrapper<T> & expose_static_methods(const std::string & js_name,
							     v8::Local<v8::ObjectTemplate> parent_template) {
	    assert(((void)"Type must be finalized before calling expose_static_methods", this->finalized) == true);

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
	    
	    return *this;
	    
	}
	

	/**
	* Used when wanting to return an object from a c++ function call back to javascript, or in conjunction with
    *   add_variable to give a javascript name to an existing c++ object 
    * \code{cpp}
    * add_variable(context, context->GetGlobal(), "js_name", class_wrapper.wrap_existing_cpp_object(context, some_c++_object));
    * \endcode
	*/
	template<class BEHAVIOR>
	v8::Local<v8::Object> wrap_existing_cpp_object(v8::Local<v8::Context> context, T * existing_cpp_object, bool force_wrap_this_type = false)
	{
		auto isolate = this->isolate;
        
        
        // if it's not finalized, try to find an existing CastToJS conversion because it's not a wrapped class
	//*** IF YOU ARE HERE LOOKING AT AN INFINITE RECURSION CHECK THE TYPE IS ACTUALLY WRAPPED ***
	if (!this->is_finalized()) {    
            // fprintf(stderr, "wrap existing cpp object cast to js %s\n", typeid(T).name());
            return CastToJS<T>()(isolate, *existing_cpp_object).template As<v8::Object>();
        }
                
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Wrapping existing c++ object %p in v8 wrapper this: %p isolate %p\n", existing_cpp_object, this, isolate);
		
		// if there's currently a javascript object wrapping this pointer, return that instead of making a new one
        //   This makes sure if the same object is returned multiple times, the javascript object is also the same
		v8::Local<v8::Object> javascript_object;
		if(this->existing_wrapped_objects.find(existing_cpp_object) != this->existing_wrapped_objects.end()) {
			if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Found existing javascript object for c++ object %p\n", existing_cpp_object);
			javascript_object = v8::Local<v8::Object>::New(isolate, this->existing_wrapped_objects[existing_cpp_object]);
			
		} else {
		
			if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Creating new javascript object for c++ object %p\n", existing_cpp_object);
		
			v8::Isolate::Scope is(isolate);
			v8::Context::Scope cs(context);

            if (this->wrap_as_most_derived_flag && !force_wrap_this_type) {
                javascript_object = this->type_checker->wrap_as_most_derived(existing_cpp_object);
            } else {
                javascript_object = get_function_template()->GetFunction()->NewInstance();

                // fprintf(stderr, "New object is empty?  %s\n", javascript_object.IsEmpty()?"yes":"no");
                // fprintf(stderr, "Created new JS object to wrap existing C++ object.  Internal field count: %d\n", javascript_object->InternalFieldCount());

                initialize_new_js_object<BEHAVIOR>(isolate, javascript_object, existing_cpp_object);

                this->existing_wrapped_objects.emplace(existing_cpp_object,
                                                       v8::Global<v8::Object>(isolate, javascript_object));
                //			if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Inserting new %s object into existing_wrapped_objects hash that is now of size: %d\n", typeid(T).name(), (int)this->existing_wrapped_objects.size());
                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Wrap existing cpp object returning object about to be cast to a value: %s\n", *v8::String::Utf8Value(javascript_object));
            }
		}
		return javascript_object;
	}


	typedef std::function<void(const v8::FunctionCallbackInfo<v8::Value>& info)> StdFunctionCallbackType;

	using AttributeAdder = std::function<void(v8::Local<v8::ObjectTemplate> &)>;
	std::vector<AttributeAdder> member_adders;

	using StaticMethodAdder = std::function<void(v8::Local<v8::FunctionTemplate>)>;
	std::vector<StaticMethodAdder> static_method_adders;


	// stores callbacks to add calls to lambdas whos first parameter is of type T* and are automatically passed
	//   the "this" pointer before any javascript parameters are passed in
	using FakeMethodAdder = std::function<void(v8::Local<v8::ObjectTemplate>)>;
	std::vector<FakeMethodAdder> fake_method_adders;

	std::string class_name = demangle<T>();

	
	template<class R, class... Params>
	V8ClassWrapper<T> & add_static_method(const std::string & method_name, R(*callable)(Params...)) {

        static std::vector<std::string> reserved_names = {"arguments", "arity", "caller", "displayName",
                                                          "length", "name", "prototype"};

        if (std::find(reserved_names.begin(), reserved_names.end(), method_name) != reserved_names.end()) {
            throw InvalidCallException(fmt::format("The name: '{}' is a reserved property in javascript functions, so it cannot be used as a static method name", method_name));
        }

		if (!std::is_const<T>::value) {
			V8ClassWrapper<typename std::add_const<T>::type>::get_instance(isolate).add_static_method(method_name, callable);
		}

		// must be set before finalization
		assert(!this->finalized);

		this->check_if_static_name_used(method_name);


		auto static_method_adder = [this, method_name, callable](v8::Local<v8::FunctionTemplate> constructor_function_template) {

		    auto static_method_function_template = v8toolkit::make_function_template(this->isolate,
											     callable);
//		    fprintf(stderr, "Adding static method %s onto %p for %s\n", method_name.c_str(), &constructor_function_template, this->class_name.c_str());
		    constructor_function_template->Set(this->isolate,
						       method_name.c_str(),
						       static_method_function_template);
		};
		
		this->static_method_adders.emplace_back(static_method_adder);
		
		return *this;
	};



	template<class Callable>
	V8ClassWrapper<T> & add_static_method(const std::string & method_name, Callable callable) {

		if (!std::is_const<T>::value) {
			V8ClassWrapper<typename std::add_const<T>::type>::get_instance(isolate).add_static_method(method_name, callable);
		}

		// must be set before finalization
		assert(!this->finalized);

		this->check_if_static_name_used(method_name);

		auto static_method_adder = [this, method_name, callable](v8::Local<v8::FunctionTemplate> constructor_function_template) {

		    auto static_method_function_template = v8toolkit::make_function_template(this->isolate,
											     callable);
		    
//		    fprintf(stderr, "Adding static method %s onto %p for %s\n", method_name.c_str(), &constructor_function_template, this->class_name.c_str());
		    constructor_function_template->Set(this->isolate,
						       method_name.c_str(),
						       static_method_function_template);
		};

		this->static_method_adders.emplace_back(static_method_adder);

		return *this;
	}

	    


    
    /**
    * Function to force API user to declare that all members/methods have been added before any
    *   objects of the wrapped type can be created to make sure everything stays consistent
    * Must be called before adding any constructors or using wrap_existing_object()
    */
	V8ClassWrapper<T> & finalize(bool wrap_as_most_derived = false);

    /**
    * returns whether finalize() has been called on this type for this isolate
    */
	bool is_finalized() {
        return this->finalized;

    }


    template<class MemberType>
    struct AttributeHelperData {
    public:
		AttributeHelperData(V8ClassWrapper<T> & class_wrapper,
                            MemberType & member_reference) :
			class_wrapper(class_wrapper), member_reference(member_reference)
		{
		    //fprintf(stderr, "Creating AttributeHelperData with %p and %p\n", (void *)&class_wrapper, (void *)&member_reference);
		}

		V8ClassWrapper<T> & class_wrapper;
		MemberType & member_reference;
    };
	template<class MemberType>
	using AttributeHelperDataCreator = std::function<AttributeHelperData<MemberType>(T*)>;


    /**
    * Adds a getter and setter method for the specified class member
    * add_member(&ClassName::member_name, "javascript_attribute_name");
    */
    // allow members from parent types of T
    template<class MEMBER_TYPE, class MemberClass, std::enable_if_t<std::is_base_of<MemberClass, T>::value, int> = 0>
	V8ClassWrapper<T> & add_member(std::string member_name,
                                   MEMBER_TYPE MemberClass::* member)
	{

	    using MEMBER_TYPE_REF = std::add_lvalue_reference_t<MEMBER_TYPE>;
	    assert(this->finalized == false);
	    
	    if (!std::is_const<T>::value) {
			V8ClassWrapper<typename std::add_const<T>::type>::get_instance(isolate).add_member_readonly(member_name, member);
	    }
	    
	    this->check_if_name_used(member_name);
	    
	    // store a function for adding the member on to an object template in the future
	    member_adders.emplace_back([this, member, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){
		    
		    auto get_attribute_helper_data =
					new AttributeHelperDataCreator<MEMBER_TYPE_REF>(
							[this, member](T * cpp_object)->AttributeHelperData<MEMBER_TYPE_REF> {

							    return AttributeHelperData<MEMBER_TYPE_REF>(*this,
                                                                            cpp_object->*member);
			});
		    
		    constructor_template->SetAccessor(v8::String::NewFromUtf8(isolate, member_name.c_str()), 
						      _getter_helper<MEMBER_TYPE_REF>, 
						      _setter_helper<MEMBER_TYPE_REF>, 
						      v8::External::New(isolate, get_attribute_helper_data));
		});
	    return *this;
	}


	// allow members from parent types of T
    template<class MEMBER_TYPE, class MemberClass, std::enable_if_t<std::is_base_of<MemberClass, T>::value, int> = 0>
	V8ClassWrapper<T> & add_member_readonly(std::string member_name,
                                            MEMBER_TYPE MemberClass::* member)
	{
	    // make sure to be using the const version even if it's not passed in
	    using ConstMemberType = typename std::add_const<MEMBER_TYPE>::type;
	    
	    // the field may be added read-only even to a non-const type, so make sure it's added to the const type, too
	    if (!std::is_const<T>::value) {
		V8ClassWrapper<typename std::add_const<T>::type>::get_instance(isolate).add_member_readonly(member_name, member);
	    }
	    
	    
	    using RESULT_REF_TYPE = typename std::conditional<std::is_const<T>::value,
	    const MEMBER_TYPE &,
	    MEMBER_TYPE &>::type;
	    
	    assert(this->finalized == false);
	    
	    this->check_if_name_used(member_name);
	    
	    member_adders.emplace_back([this, member, member_name](v8::Local<v8::ObjectTemplate> & constructor_template){


		    auto get_attribute_helper_data = new AttributeHelperDataCreator<ConstMemberType>([this, member](T * cpp_object)->AttributeHelperData<ConstMemberType>{
			    return AttributeHelperData<ConstMemberType>(*this, cpp_object->*member);
			});
		    
		    constructor_template->SetAccessor(v8::String::NewFromUtf8(isolate, member_name.c_str()), 
						      _getter_helper<ConstMemberType>,
						      0,
						      v8::External::New(isolate, get_attribute_helper_data));
		});
	    return *this;
	}


	/**
	* Adds the ability to call the specified class instance function when the javascript is called as `my_object();`
	*/
	template<class R, class TBase, class... Args,
			 std::enable_if_t<std::is_same<TBase,T>::value || std::is_base_of<TBase, T>::value, int> = 0>
	V8ClassWrapper<T> & make_callable(R(TBase::*method)(Args...))
	{
	    return _add_method("unused name", method, true);
	}
    
    
    
	template<class R, class TBase, class... Args,
			 std::enable_if_t<std::is_base_of<TBase, T>::value, int> = 0>
	V8ClassWrapper<T> & add_method(const std::string & method_name, R(TBase::*method)(Args...) const) {
	    if (!std::is_const<T>::value) {
		V8ClassWrapper<std::add_const_t<T>>::get_instance(isolate)._add_method(method_name, method);
	    }
	    return _add_method(method_name, method);
	}

    
	/**
	* Adds the ability to call the specified class instance method on an object of this type
	*/
	template<class R, class TBase, class... Args,
			 std::enable_if_t<std::is_same<TBase,T>::value || std::is_base_of<TBase, T>::value, int> = 0>
	V8ClassWrapper<T> & add_method(const std::string & method_name, R(TBase::*method)(Args...))
	{
		return _add_method(method_name, method);
	}


	/**
	* If the method is marked const, add it to the const version of the wrapped type
	*/
	template<class R, class Head, class... Tail, std::enable_if_t<std::is_const<Head>::value && !std::is_const<T>::value, int> = 0>
	void add_fake_method_for_const_type(const std::string & method_name, std::function<R(Head, Tail...)> method) {
		V8ClassWrapper<typename std::add_const<T>::type>::get_instance(isolate).add_fake_method(method_name, method);
	};


	/**
	 * If the method is not marked const, don't add it to the const type (since it's incompatible)
	 */
	template<class R, class Head, class... Tail, std::enable_if_t<!(std::is_const<Head>::value && !std::is_const<T>::value), int> = 0>
	void add_fake_method_for_const_type(const std::string & method_name, std::function<R(Head, Tail...)> method) {
		// nothing to do here
	};


    template<class R, class... Args>
	void add_method(const std::string & method_name, std::function<R(T*, Args...)> & method) {
		_add_fake_method(method_name, method);
	}


	/**
	 * Takes a lambda taking a T* as its first parameter and creates a 'fake method' with it
	 */
	template<class Callback>
	V8ClassWrapper<T> & add_method(const std::string & method_name, Callback && callback) {
		decltype(LTG<Callback>::go(&Callback::operator())) f(callback);
		this->_add_fake_method(method_name, f);

		return *this;
	}


	v8::Local<v8::Object> wrap_as_most_derived(T * cpp_object) {
		return this->type_checker->wrap_as_most_derived(cpp_object);
	}

	template<class R, class Head, class... Tail>
	V8ClassWrapper<T> & _add_fake_method(const std::string & method_name, std::function<R(Head, Tail...)> method)
	{
		assert(this->finalized == false);

		add_fake_method_for_const_type(method_name, method);

		this->check_if_name_used(method_name);


		// This puts a function on a list that creates a new v8::FunctionTemplate and maps it to "method_name" on the
		// Object template that will be passed in later when the list is traversed
		fake_method_adders.emplace_back([this, method_name, method](v8::Local<v8::ObjectTemplate> prototype_template) {

			using CopyFunctionType = std::function<R(Head, Tail...)>;
			CopyFunctionType * copy = new std::function<R(Head, Tail...)>(method);


			// This is the actual code associated with "method_name" and called when javascript calls the method
			StdFunctionCallbackType * method_caller =
					new StdFunctionCallbackType([method_name, copy](const v8::FunctionCallbackInfo<v8::Value>& info) {


				auto fake_method = *(std::function<R(Head, Tail...)>*)v8::External::Cast(*(info.Data()))->Value();
				auto isolate = info.GetIsolate();

				auto holder = info.Holder();


				v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(holder->GetInternalField(0));
				WrappedData<T> * wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
				T * cpp_object = V8ClassWrapper<T>::get_instance(isolate).cast(static_cast<AnyBase *>(wrapped_data->native_object));


				// the typelist and std::function parameters don't match because the first parameter doesn't come
				// from the javascript value array in 'info', it is passed in from this function as the 'this' pointer
//				using PB_TYPE = v8toolkit::ParameterBuilder<0, std::function<R(Head, Tail...)>, TypeList<Tail...>>;
//
//				PB_TYPE pb;
//				auto arity = PB_TYPE::ARITY;
				// 1  because the first parameter doesn't count because it's reserved for "this"
//				if (!check_parameter_builder_parameter_count<PB_TYPE, 0>(info)) {
//				  std::stringstream ss;
//				  ss << "Function '" << method_name << "' called from javascript with insufficient parameters.  Requires " << arity << " provided " << info.Length();
//				  isolate->ThrowException(v8::String::NewFromUtf8(isolate, ss.str().c_str()));
//				  return; // return now so the exception can be thrown inside the javascript
//				}

				// V8 does not support C++ exceptions, so all exceptions must be caught before control
				//   is returned to V8 or the program will instantly terminate
				try {
					CallCallable<CopyFunctionType, Head>()(*copy, info, cpp_object);
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
		return *this;
	}

	/**
	 * A list of methods to be added to each object
	 */

	struct MethodAdderData {
	    std::string method_name;
	    StdFunctionCallbackType callback;
	};
	// Nothing may ever be removed from this vector, as things point into it
	std::list<MethodAdderData> method_adders;

	// makes a single function to be run when the wrapping javascript object is called with ()
	MethodAdderData callable_adder;

    template<class M>
	V8ClassWrapper<T> & _add_method(const std::string & method_name, M method, bool add_as_callable_object_callback = false)
    {
        assert(this->finalized == false);

		this->check_if_name_used(method_name);

		
    		MethodAdderData method_adder_data = {method_name, StdFunctionCallbackType([this, method, method_name](const v8::FunctionCallbackInfo<v8::Value>& info) {
                auto isolate = info.GetIsolate();

                // get the behind-the-scenes c++ object
                // However, Holder() refers to the most-derived object, so the prototype chain must be
                //   inspected to find the appropriate v8::Object with the T* in its internal field
                auto holder = info.Holder();
                v8::Local<v8::Object> self;

                if (V8_CLASS_WRAPPER_DEBUG)
                    fprintf(stderr, "Looking for instance match in prototype chain %s :: %s\n", demangle<T>().c_str(),
                            demangle<M>().c_str());
                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Holder: %s\n", stringify_value(isolate, holder).c_str());
                if (V8_CLASS_WRAPPER_DEBUG) dump_prototypes(isolate, holder);


                auto function_template_count = this->this_class_function_templates.size();
                int current_template_count = 0;
                for (auto &function_template : this->this_class_function_templates) {
                    current_template_count++;
                    if (V8_CLASS_WRAPPER_DEBUG)
                        fprintf(stderr, "Checking function template %d / %d\n", current_template_count,
                                (int) function_template_count);
                    self = holder->FindInstanceInPrototypeChain(function_template.Get(isolate));
                    if (!self.IsEmpty() && !self->IsNull()) {
                        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Found instance match in prototype chain\n");
                        break;
                    } else {
                        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "No match on this one\n");
                    }
                }
                if (self.IsEmpty()) {
                    if (V8_CLASS_WRAPPER_DEBUG)
                        fprintf(stderr,
                                "No match in prototype chain after looking through all potential function templates\n");
                    assert(false);
                }
                //
                // if(!compare_contents(isolate, holder, self)) {
                //     fprintf(stderr, "FOUND DIFFERENT OBJECT");
                // }
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Done looking for instance match in prototype chain\n");
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Match: %s:\n", *v8::String::Utf8Value(self));
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "%s\n", stringify_value(isolate, self).c_str());
//                assert(!self.IsEmpty());

                // void* pointer = instance->GetAlignedPointerFromInternalField(0);
                auto wrap = v8::Local<v8::External>::Cast(self->GetInternalField(0));

//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "uncasted internal field: %p\n", wrap->Value());
                WrappedData<T> *wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());
                auto backing_object_pointer = V8ClassWrapper<T>::get_instance(isolate).cast(
                    static_cast<AnyBase *>(wrapped_data->native_object));

//			    assert(backing_object_pointer != nullptr);
                // bind the object and method into a std::function then build the parameters for it and call it
//                if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "binding with object %p\n", backing_object_pointer);
                auto bound_method = v8toolkit::bind<T>(*backing_object_pointer, method);


//                using PB_TYPE = v8toolkit::ParameterBuilder<0, decltype(bound_method), decltype(get_typelist_for_function(bound_method))>;
//
//                PB_TYPE pb;
//                auto arity = PB_TYPE::ARITY;
//                if (!check_parameter_builder_parameter_count<PB_TYPE, 0>(info)) {
//                    std::stringstream ss;
//                    ss << "Function '" << method_name << "' called from javascript with insufficient parameters.  Requires " << arity << " provided " << info.Length();
//                    isolate->ThrowException(v8::String::NewFromUtf8(isolate, ss.str().c_str()));
//                    return; // return now so the exception can be thrown inside the javascript
//                }

                // V8 does not support C++ exceptions, so all exceptions must be caught before control
                //   is returned to V8 or the program will instantly terminate
                try {
                    // if (dynamic_cast< JSWrapper<T>* >(backing_object_pointer)) {
                    //     dynamic_cast< JSWrapper<T>* >(backing_object_pointer)->called_from_javascript = true;
                    // }
                    CallCallable<decltype(bound_method)>()(bound_method, info);
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

        return *this;
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
    std::map<v8::Isolate *, V8ClassWrapper<T> *> V8ClassWrapper<T, V8TOOLKIT_V8CLASSWRAPPER_USE_REAL_TEMPLATE_SFINAE>::isolate_to_wrapper_map;

template<class T>
class JSWrapper;


template<typename T, class>
struct CastToJS {

    
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T & cpp_object){
	    if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS from lvalue ref %s\n", demangle<T>().c_str());
		return CastToJS<typename std::add_pointer<T>::type>()(isolate, &cpp_object);
	}

	/**
	* If an rvalue is passed in, a copy must be made.
	*/
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T && cpp_object){
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "In base cast to js struct with rvalue ref");
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "Asked to convert rvalue type, so copying it first\n");

		// this memory will be owned by the javascript object and cleaned up if/when the GC removes the object
		auto copy = new T(cpp_object);
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);
		auto result = class_wrapper.template wrap_existing_cpp_object<DestructorBehavior_Delete<T>>(context, copy);
        if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS<T> returning wrapped existing object: %s\n", *v8::String::Utf8Value(result));
        
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
		return v8::Local<v8::Object>();
	    }

	    assert(cpp_object != (void *)0xbebebebebebebebe);
	    
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS from T* %s\n", demangle_typeid_name(typeid(T).name()).c_str());
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

#ifdef V8TOOLKIT_BIDIRECTIONAL_ENABLED
		// if the type is polymorphic and potentially bidirectional, check to see if it actually is
		using JSWrapperType = JSWrapper<std::remove_const_t<T>>;
//		fprintf(stderr, "Checking to see if object * is a JSWrapper *\n");
		auto js_wrapper_t = dynamic_cast<const JSWrapperType *>(cpp_object);
		if (js_wrapper_t) {
		    return CastToJS<JSWrapperType>()(isolate, *js_wrapper_t);
		}
#endif
		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

		/** If you are here looking for an INFINITE RECURSION make sure the type is wrapped **/
		return class_wrapper.template wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, cpp_object);
	}
};

template<typename T>
struct CastToJS<T*, std::enable_if_t<!std::is_polymorphic<T>::value>> {
	v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * cpp_object){

	    if (cpp_object == nullptr) {
		return v8::Local<v8::Object>();
	    }
	    assert(cpp_object != (void *)0xbebebebebebebebe);


	    if (V8_CLASS_WRAPPER_DEBUG) std::cout << fmt::format("CastToJS from T* {}\n", demangle<T>()) << std::endl;
		auto context = isolate->GetCurrentContext();
		V8ClassWrapper<T> & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

		return class_wrapper.template wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, cpp_object);
	}

 };

 template<typename T>
     struct CastToJS<T * const> {
     v8::Local<v8::Value> operator()(v8::Isolate * isolate, T * const cpp_object){

         if (cpp_object == nullptr) {
             return v8::Local<v8::Object>();
         }
         assert(cpp_object != (void *)0xbebebebebebebebe);

         if (V8_CLASS_WRAPPER_DEBUG) std::cout << fmt::format("CastToJS from T* {}\n", demangle<T>()) << std::endl;
         auto context = isolate->GetCurrentContext();
         auto & class_wrapper = V8ClassWrapper<T>::get_instance(isolate);

         if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

         return class_wrapper.template wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, cpp_object);
     }
 };


    template<typename T>
    struct CastToJS<T const * const> {
        v8::Local<v8::Value> operator()(v8::Isolate * isolate, T const * cpp_object){

            if (cpp_object == nullptr) {
                return v8::Local<v8::Object>();
            }
            assert(cpp_object != (void *)0xbebebebebebebebe);

            if (V8_CLASS_WRAPPER_DEBUG) std::cout << fmt::format("CastToJS from T* {}\n", demangle<T>()) << std::endl;
            auto context = isolate->GetCurrentContext();
            auto & class_wrapper = V8ClassWrapper<T const>::get_instance(isolate);

            if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "CastToJS<T*> returning wrapped existing object for %s\n", typeid(T).name());

            return class_wrapper.template wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, cpp_object);
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

		if (V8_CLASS_WRAPPER_DEBUG) fprintf(stderr, "cast to native\n");
		if(!value->IsObject()){
			fprintf(stderr, "CastToNative failed for type: %s (%s)\n", type_details<T>().c_str(), *v8::String::Utf8Value(value));
			throw CastException("No specialized CastToNative found and value was not a Javascript Object");
		}
		auto object = v8::Object::Cast(*value);
		if (object->InternalFieldCount() <= 0) {
			throw CastException(fmt::format("No specialization CastToNative<{}> found (for any shortcut notation) and provided Object is not a wrapped C++ object.  It is a native Javascript Object", demangle<T>()));
		}
		v8::Local<v8::External> wrap = v8::Local<v8::External>::Cast(object->GetInternalField(0));
		auto wrapped_data = static_cast<WrappedData<T> *>(wrap->Value());

		auto any_base = (v8toolkit::AnyBase *)wrapped_data->native_object;
		T * t = nullptr;
		// std::cerr << fmt::format("about to call cast on {}", demangle<T>()) << std::endl;
		if ((t = V8ClassWrapper<T>::get_instance(isolate).cast(any_base)) == nullptr) {
//			fprintf(stderr, "Failed to convert types: want:  %d %s, got: %s\n", std::is_const<T>::value, typeid(T).name(), TYPE_DETAILS(*any_base));
			throw CastException(fmt::format("Cannot convert {} to {} {}",
							TYPE_DETAILS(*any_base), std::is_const<T>::value, demangle<T>()));
		}
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



	// If no more-derived option was found, wrap as this type
	template<class T>
	v8::Local<v8::Object> TypeChecker<T, v8toolkit::TypeList<>>::wrap_as_most_derived(T * cpp_object) const {
		auto context = this->isolate->GetCurrentContext();
		return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate).template wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, cpp_object, true /* don't infinitely recurse */);
	}


	template<class T, class Head, class... Tail> T *
TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,
std::enable_if_t<!(!std::is_const<T>::value && std::is_const<Head>::value)>>::check(AnyBase * any_base, bool first_call) const {

	if(AnyPtr<Head> * any = dynamic_cast<AnyPtr<Head> *>(any_base)) {
#ifdef ANYBASE_DEBUG
		std::cerr << fmt::format("Got match on: {}", demangle<Head>()) << std::endl;
#endif
		return static_cast<T*>(any->get());
	}

	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
	    if (auto derived_result = V8ClassWrapper<Head>::get_instance(this->isolate).cast(any_base)) {
		return derived_result;
	    }
	}

	return SUPER::check(any_base, false);
}




// if a more-derived type was found, pass it to that type to see if there's something even more derived
template<class T, class Head, class... Tail>
v8::Local<v8::Object> TypeChecker<T, v8toolkit::TypeList<Head, Tail...>,
	std::enable_if_t<!(!std::is_const<T>::value && std::is_const<Head>::value)>>
::wrap_as_most_derived(T * cpp_object) const {

	// if they're the same, let it fall through to the empty typechecker TypeList base case
	if (!std::is_same<std::remove_const_t<T>, std::remove_const_t<Head>>::value) {
		using MatchingConstT = std::conditional_t<std::is_const<Head>::value, std::add_const_t<T>, std::remove_const_t<T>>;

//		fprintf(stderr, "Head is polymorphic? %s, %d\n", demangle<Head>().c_str(),
//				std::is_polymorphic<Head>::value);
//		fprintf(stderr, "MatchingConstT is polymorphic?  %s, %d\n", demangle<MatchingConstT>().c_str(),
//				std::is_polymorphic<MatchingConstT>::value);

		if (std::is_const<T>::value == std::is_const<Head>::value) {
			if (auto derived = safe_dynamic_cast<Head *>(const_cast<MatchingConstT *>(cpp_object))) {
				return v8toolkit::V8ClassWrapper<Head>::get_instance(this->isolate).wrap_as_most_derived(derived);
			}
		}
	}
	return SUPER::wrap_as_most_derived(cpp_object);
}



} // end namespace v8toolkit

