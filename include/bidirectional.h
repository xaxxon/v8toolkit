#pragma once

#include <string>

#define V8TOOLKIT_BIDIRECTIONAL_ENABLED

#include "v8toolkit.h"






/**
* This file contains things needed to create "types" and objects in both C++ and javascript
*   and use them in either environment regardless of where they were created.  Since C++ 
*   types must exist at compile time, types created with these tools cannot create
*   actual C++ types, but instead allow behavior to be overridden on virtual functions
*
* See samples/bidirectional_sample.cpp for examples on how to use this library.
*/


namespace v8toolkit {

class BidirectionalException : public std::exception {
    std::string reason;
public:
    BidirectionalException(const std::string &reason) : reason(reason) { }

    virtual const char *what() const noexcept { return reason.c_str(); }
};


// Inheriting from this gives hints to the system that you are a bidirectional type
class JSWrapperBase {
protected:
    v8::Isolate *isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Object> global_js_object;
    v8::Global<v8::FunctionTemplate> global_created_by;

    /**
    * It's easy to end up in infinite recursion where the JSWrapper object looks for a javascript implementation
    *   to call instead of calling the native C++ implementation, but finds its own JS_ACCESS function that its already in
    *   an proceeds to call itself.  This flag stops that from happening - at least in naive situations.   Marked as
    *   mutable because it needs to be changed even from within const methods
    */
    mutable bool called_from_javascript = false;

protected:
    JSWrapperBase(v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
            v8::Local<v8::FunctionTemplate> created_by) :
    isolate(context->GetIsolate()),
    global_context(v8::Global<v8::Context>(isolate, context)),
            global_js_object(v8::Global<v8::Object>(isolate, object)),
    global_created_by(v8::Global<v8::FunctionTemplate>(isolate, created_by)) { }

public:
    v8::Local<v8::Object> get_javascript_object() const { return global_js_object.Get(isolate); }

};


/**
* C++ types to be extended in javascript must inherit from this class.
* Example: class MyClass{};  class JSMyClass : public MyClass, public JSWrapper {};
*   Now, JSMyClass can be used as a MyClass, but will intercept calls to its methods
*   and attempt to use the javascript object to fulfill them, falling back to the
*   C++ class methods of MyClass when necessary
* Any class inheriting from this (e.g. JSMyClass) must have the first two parameters of its constructor
*   be a v8::Local<v8::Context>, v8::Local<v8::Object>
*/
template<class Base>
class JSWrapper : public JSWrapperBase {
protected:
    using BASE_TYPE = Base;

public:
    JSWrapper(v8::Local<v8::Context> context,
              v8::Local<v8::Object> object,
              v8::Local<v8::FunctionTemplate> created_by) :
        JSWrapperBase(context, object, created_by)
    {}
};

template<class T>
std::enable_if_t<std::is_base_of<JSWrapperBase, T>::value, v8::Local<v8::Object>> safe_get_javascript_object(T * object) {
    return object->get_javascript_object();
}

template<class T>
std::enable_if_t<!std::is_base_of<JSWrapperBase, T>::value, v8::Local<v8::Object>> safe_get_javascript_object(T * object) {
    return v8::Local<v8::Object>();
}




/* template<class... Ts> */
/* struct CastToJS<JSWrapper<Ts...>> { */
/*     v8::Local<v8::Value> operator()(v8::Isolate *isolate, JSWrapper<Ts...> &js_wrapper) { */
/* //        printf("Using custom JSWrapper CastToJS method"); */
/*         return js_wrapper.get_javascript_object(); */
/*     } */
/* }; */
template<class T>
struct CastToJS<JSWrapper<T>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, const JSWrapper<T> &js_wrapper) {
//        printf("Using custom JSWrapper CastToJS method");
        return js_wrapper.get_javascript_object();
    }
};
} // end v8toolkit namespace

#include "v8_class_wrapper.h"

#ifndef V8_CLASS_WRAPPER_HAS_BIDIRECTIONAL_SUPPORT
#error bidirectional.h must be included before v8_class_wrapper.h
#endif

namespace v8toolkit {

    /** 
     * Class for Factory to inherit from when no other parent is specified
     * Must be empty
     */
 class EmptyFactoryBase {};




/**
 * Common Factory Inheritance goes:
 * v8toolkit::CppFactory => custom user-defined Factory type for additional data/functions =>
 *     v8toolkit::Factory => v8toolkit::Factory<BaseType> => Common base type for all factories
 *
 */

/**
 * Internal constructor parameters are parameters only on the Base type constructor that are not specified to
 *   each instance of the derived type.  The value is fixed across all isntances of the specific derived type.
 *
 * External constructor parameters are parameers are all the parameters to the derived type (except an optional
 *   first parameter of the factory object which will be automatically populated)
 */

    
/**
* Base class for a Factory that creates a bidirectional "type" by name.  The object
*   returned can be used as a class T regardless of whether it is a pure C++ type 
*   or if it has been extended in javascript
*/
template<class Base, // The common base class type of object to be returned
         class = TypeList<>, // List of parameters that may change per-instance created
         class FactoryBase = EmptyFactoryBase> // base class of this factory class
class Factory;


 template<class Base, class... ConstructorArgs, class FactoryBase>
     class Factory<Base, TypeList<ConstructorArgs...>, FactoryBase> : public virtual FactoryBase {
public:
//     Factory(ParentTs&&... parent_ts) : ParentType(std::forward<ParentTs>(parent_ts)...)
//     {}
     Factory() = default;
     Factory(const Factory &) = delete;
     Factory(Factory &&) = default;
     Factory & operator=(const Factory &) = delete;
     Factory & operator=(Factory &&) = default;


    /**
     * Returns a pointer to a new object inheriting from type Base
     */
    virtual Base * operator()(ConstructorArgs&&... constructor_args) const = 0;

    Base * create(ConstructorArgs&&... constructor_args) const {return this->operator()(std::forward<ConstructorArgs>(constructor_args)...);}

    /**
     * Returns a unique_ptr to a new object inheriting from type Base
     */
    template <class U = Base>
    std::unique_ptr<U> get_unique(ConstructorArgs&&... args) const {

	// call operator() on the factory and put the results in a unique pointer
        return std::unique_ptr<U>((*this)(std::forward<ConstructorArgs>(args)...));
    }

    /**
     * Helper to quickly turn a Base type into another type if allowed
     */
    template<class U, class... Args>
    U * as(Args&&...  args) const {
        // printf("Trying to cast a %s to a %s\n", typeid(Base).name(), typeid(U).name());
        auto result = this->operator()(std::forward<Args>(args)...);
        if (dynamic_cast<U*>(result)) {
            return static_cast<U*>(result);
        } else {
            throw BidirectionalException("Could not convert between types");
        }
    }
};


/**
* Returns a pure-C++ object of type Child which inherits from type Base.  It's Base type and ConstructorArgs... 
*   must match with the Factory it is associated with.  You can have it inherit from a type that inherits from v8toolkit::Factory
*   but v8toolkit::Factory must be in the inheritance chain somewhere
*/
template<
	class Base,
	class Child,
        class FixedParamsTypeList, // parameters to be sent to the constructor that are known at factory creation time
	class ExternalTypeList,
    class FactoryBase = Factory<Base, ExternalTypeList, EmptyFactoryBase>>
class CppFactory;


// if the constructor wants a reference to the factory, automatically pass it in
 template<class Base, class Child, class... ExternalConstructorParams, class... FixedParams, class FactoryBase>
    class CppFactory<Base,
                    Child,
                    TypeList<FixedParams...>,
                    TypeList<ExternalConstructorParams...>,
     FactoryBase> :
 public virtual FactoryBase {

     using TupleType = std::tuple<FixedParams...>;
     TupleType fixed_param_tuple;
     
 public:
     
     
     
 CppFactory(FixedParams&&... fixed_param_values) :
     fixed_param_tuple(fixed_param_values...)
	 {}
     
     CppFactory(const CppFactory &) = delete;
     CppFactory(CppFactory &&) = default;
     CppFactory & operator=(const CppFactory &) = delete;
     CppFactory & operator=(CppFactory &&) = default;


    template<std::size_t... Is>
	Base * call_operator_helper(ExternalConstructorParams&&... constructor_args, std::index_sequence<Is...>) const {

	// must const cast it since this method is const, so the tuple becomes const
	return new Child(std::forward<FixedParams>(std::get<Is>(const_cast<TupleType&>(fixed_param_tuple)))...,
			 std::forward<ExternalConstructorParams>(constructor_args)...);
    }
     
     virtual Base * operator()(ExternalConstructorParams&&... constructor_args) const override {
	 return call_operator_helper(std::forward<ExternalConstructorParams>(constructor_args)...,
				     std::index_sequence_for<FixedParams...>());
    }
};


/**
* Returns a JavaScript-extended object inheriting from Base.  It's Base type and
*   *ConstructorParams must match up with the Factory class it is associated
*
* InternalConstructorParams are ones that will be specified in the javascript code declaring the new type
*
* ExternalConstructorParams will be potentially change for each instance of that type
*
* Example of internal vs external parameters:  if the base type is "animal" and it takes two parameters
*    "is_mammal" and "name".   Whether or not the derived type is a mammal is known when making the derived type
*    so it would be an internal parameter, while the name isn't known until the object is constructed so it would
*    be an external parameter.
*
*  Perhaps the order should be swapped to take external first, since that is maybe more common?
*/
    template<
	class Base,
	class JSWrapperClass,

	class Internal,
	class External,
    class FactoryBase = Factory<Base, External, EmptyFactoryBase>>
class JSFactory; // instance of undefined template means your inheritance is wrong and failing sfinae check
    // sfinae check is almost certainly good

// Begin real specialization
template<
	class Base,
	class JSWrapperClass,

	class... InternalConstructorParams,
	class... ExternalConstructorParams,
    class FactoryBase>
class JSFactory<
	Base,
	JSWrapperClass,

	TypeList<InternalConstructorParams...>,
	TypeList<ExternalConstructorParams...>,
    FactoryBase>

	: public virtual FactoryBase
{ // Begin JSFactory class

	using ThisFactoryType = JSFactory<Base, JSWrapperClass, TypeList<InternalConstructorParams...>, TypeList<ExternalConstructorParams...>, FactoryBase>;
    
    
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    // Create base portion of new object using wrapped type
    v8::Global<v8::FunctionTemplate> js_constructor_function;

    // javascript callback for creating properties only on the new type
    v8::Global<v8::Function> js_new_object_constructor_function;
    
    v8::Global<v8::Object> js_prototype;

    func::function<JSWrapperClass * (ExternalConstructorParams&&...)> make_jswrapper_object;

    using TupleType = std::tuple<InternalConstructorParams...>;
    TupleType internal_param_tuple;


public:

    /**
     * Helper function for creating JSFactory objects from javascript
     */
    template<int starting_info_index, std::size_t... Is>
    static std::unique_ptr<ThisFactoryType> _create_factory_from_javascript(const v8::FunctionCallbackInfo<v8::Value> & info, std::index_sequence<Is...>) {

        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();

        // info.Length() must be sum of any skipped parameters + 2 for the prototype and object callback + 1 for each InternalConstructorParameters
        constexpr std::size_t parameter_count = starting_info_index + 2 + sizeof...(InternalConstructorParams);
        if (info.Length() != parameter_count) {
            throw InvalidCallException(fmt::format("Wrong number of parameters to create new factory - needs {}, got {}", parameter_count, info.Length()));
        }

        int i = starting_info_index + 2; // skip the prototype object and object constructor callback as well
        std::vector<std::unique_ptr<v8toolkit::StuffBase>> stuff;

        // create unique_ptr with new factory
        return run_function<
                std::unique_ptr<ThisFactoryType>, // return type
                decltype(context),            // first parameter type for function
                decltype(info[starting_info_index + 0]->ToObject()), // second parameter type for function
                v8::Local<v8::Function>,
                InternalConstructorParams...> // constructor params for function
                (&std::make_unique<ThisFactoryType, // specify exactly which make_unique to give a function pointer to
                         decltype(context),   // repeat types for make_unique
                         decltype(info[starting_info_index + 0]->ToObject()), // repeat types for make_unique
                         v8::Local<v8::Function>,
                         InternalConstructorParams...>, // repeat types for make_unique
                 info, // run_function needs the info object
                 context, // all the rest are the parameters to the actual function call
                 info[starting_info_index + 0]->ToObject(),
                 v8::Local<v8::Function>::Cast(info[starting_info_index + 1]),
                 ParameterBuilder<InternalConstructorParams>()(info, i, stuff)...);

    }


    template<std::size_t... Is>
    std::unique_ptr<JSWrapperClass> call_operator_helper(v8::Local<v8::Object> new_js_object,
                                ExternalConstructorParams&&... constructor_args,
                                std::index_sequence<Is...>) const {

        return std::make_unique<JSWrapperClass>(this->global_context.Get(isolate),
						new_js_object,
						this->js_constructor_function.Get(isolate), // the v8::FunctionTemplate that created the js object

						// must const cast it since this method is const, so the tuple becomes const
						std::forward<InternalConstructorParams>(std::get<Is>(const_cast<TupleType &>(this->internal_param_tuple)))...,
						std::forward<ExternalConstructorParams>(constructor_args)...);
    }


        /**
    * Takes a context to use while calling a javascript_function that returns an object
    *   inheriting from JSWrapper
    */
    JSFactory(v8::Local<v8::Context> context, v8::Local<v8::Object> prototype, v8::Local<v8::Function> js_new_object_constructor_function, InternalConstructorParams&&... internal_constructor_values) :
        isolate(context->GetIsolate()),
        global_context(v8::Global<v8::Context>(isolate, context)),

	    // use the Base type's javascript wrapper type so that all its added data members are available
	    js_constructor_function(v8::Global<v8::FunctionTemplate>(isolate, V8ClassWrapper<Base>::get_instance(isolate).get_function_template())),
        js_new_object_constructor_function(v8::Global<v8::Function>(isolate, js_new_object_constructor_function)),
        js_prototype(v8::Global<v8::Object>(isolate, prototype)),
        internal_param_tuple(internal_constructor_values...)

    {
//        printf("Created JSFactory object at %p\n", (void*)this);



        // Create a new object of the full wrapper type (i.e. JSMyType : MyType, JSWrapper<MyTYpe>)
        auto new_js_object = js_constructor_function.Get(isolate)->GetFunction()->NewInstance();
        (void) this->js_prototype.Get(isolate)->SetPrototype(context, new_js_object->GetPrototype());

        // create a callback for making a new object using the internal constructor values provided here - external ones provided at callback time
        // DO NOT CAPTURE/USE ANY V8::LOCAL VARIABLES IN HERE, only use v8::Global::Get(...)
        this->make_jswrapper_object = [this](ExternalConstructorParams&&... external_constructor_values) mutable ->JSWrapperClass * {
//            printf("Using JSFactory object at %p\n", (void*)this);

            auto context = this->global_context.Get(this->isolate);

            // Create a new javascript object for Base but then set its prototype to the subclass's prototype
            v8::Local<v8::Object> new_js_object = this->js_constructor_function.Get(isolate)->GetFunction()->NewInstance();
            (void)new_js_object->SetPrototype(context, this->js_prototype.Get(isolate));

            auto js_wrapper_class_cpp_object =
                call_operator_helper(new_js_object,
                                     std::forward<ExternalConstructorParams>(external_constructor_values)...,
                                     std::index_sequence_for<InternalConstructorParams...>());
            /*
            auto js_wrapper_class_cpp_object = std::make_unique<JSWrapperClass>(this->global_context.Get(isolate),
                                                    new_js_object,
                                                    this->js_constructor_function.Get(isolate), // the v8::FunctionTemplate that created the js object
                                                    *this,
                                                    std::forward<InternalConstructorParams>(internal_constructor_values)...,
                                                    std::forward<ExternalConstructorParams>(external_constructor_values)...);
            */
            auto & wrapper = V8ClassWrapper<JSWrapperClass>::get_instance(isolate);
            wrapper.template initialize_new_js_object(isolate, new_js_object, js_wrapper_class_cpp_object.get(), *wrapper.destructor_behavior_delete);

            // call javascript "constructor" method (per-instance)
            v8toolkit::call_javascript_function_with_vars(context,
                                                          this->js_new_object_constructor_function.Get(isolate),
                                                          context->Global(),
                                                          TypeList<v8::Local<v8::Object>>(),
                                                          new_js_object);

            return js_wrapper_class_cpp_object.release();
        };
    }

    ~JSFactory(){
//        printf("Deleting JSFactory object at %p\n", (void*)this);
    }

    

    /**
     * Returns a C++ object inheriting from JSWrapper that wraps a newly created javascript object which
     *   extends the C++ functionality in javascript
     */
    virtual Base * operator()(ExternalConstructorParams&&... constructor_parameters) const override {
       return this->make_jswrapper_object(std::forward<ExternalConstructorParams>(constructor_parameters)...);
    }


    template<int starting_info_index>
    static std::unique_ptr<ThisFactoryType> create_factory_from_javascript(const v8::FunctionCallbackInfo<v8::Value> & info) {
	    return _create_factory_from_javascript<starting_info_index>(info, std::index_sequence_for<InternalConstructorParams...>());
    }

    
    static void wrap_factory(v8::Isolate * isolate) {
	V8ClassWrapper<ThisFactoryType> & wrapper = V8ClassWrapper<ThisFactoryType>::get_instance(isolate);
        wrapper.add_method("create", &ThisFactoryType::operator());
        wrapper.finalize();
    }
 };




// turn on/off print statements for helping debug JS_ACCESS functionality
#define JS_ACCESS_CORE_DEBUG false

/**
* This code looks for a javascript method on the JavaScript object contained
*   in the "this" JSWrapper object and call the "name"d method on it.  It must work
*   when this method is called directly to start the method call (using a bidirectional 
*   object from C++) as well as when the method call is started from javascript (where the
*   javascript interpreter checks the prototype chain initially and might find this function)
*   If not careful, this function can find itself while looking for a javascript version to call
*   because even though its methods aren't mapped into javascript, the parent type's are and 
*   dynamic dispatch will call the derived class's version instead of the base class.
*   That is why static dispatch is specifically used for the C++ fallback case:
*   `this->BASE_TYPE::name( __VA_ARGS__ );`
*/ 
#define JS_ACCESS_CORE(ReturnType, name, ...) \
    bool call_native = this->called_from_javascript; \
    \
    /* If there is infinite recursion happening here, it is likely                      */ \
    /*   that when this function looks for a JavaScript function to call                */ \
    /*   it actually finds this function and calls itself.  This flag is supposed       */ \
    /*   to protect against that but the situations are complicated and this may not be */ \
    /*   sufficient. */ \
    this->called_from_javascript = false; \
    if (call_native) { \
        if(JS_ACCESS_CORE_DEBUG) printf("Calling native version of %s\n", #name); \
	/* See comment above for why static dispatch is used */		\
        return this->BASE_TYPE::name( __VA_ARGS__ );			\
    } \
	if(JS_ACCESS_CORE_DEBUG) printf("IN JS_ACCESS_CORE for %s, not calling native code\n", #name); \
    /*auto parameter_tuple = std::make_tuple( __VA_ARGS__ ); */ \
   /* auto parameter_tuple = make_tuple_for_variables(__VA_ARGS__); */ \
    v8toolkit::CastToNative<std::remove_reference<ReturnType>::type> cast_to_native; \
    GLOBAL_CONTEXT_SCOPED_RUN(isolate, global_context); \
    auto context = global_context.Get(isolate); \
    auto js_object = global_js_object.Get(isolate); \
    v8::Local<v8::Function> js_function; \
    v8::TryCatch tc(isolate); \
    try { \
        js_function = v8toolkit::get_key_as<v8::Function>(context, js_object, #name); \
    } catch (...) {assert(((void)"method probably not added to wrapped parent type", false) == true);} \
    this->called_from_javascript = true; \
    auto result = v8toolkit::call_javascript_function_with_vars(context, js_function, js_object, typelist, ##__VA_ARGS__); \
    this->called_from_javascript = false; \
    return cast_to_native(isolate, result); \

// defines a JS_ACCESS function for a method taking no parameters
#define JS_ACCESS(return_type, name)\
virtual return_type name() override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_0(return_type, name)\
virtual return_type name() override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_1(return_type, name, t1)\
virtual return_type name(t1 p1) override {\
    v8toolkit::TypeList<t1> typelist; \
    JS_ACCESS_CORE(return_type, name, p1)\
}

#define JS_ACCESS_2(return_type, name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) override {\
    v8toolkit::TypeList<t1, t2> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2)\
}

#define JS_ACCESS_3(return_type, name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) override {\
     v8toolkit::TypeList<t1, t2, t3> typelist; \
JS_ACCESS_CORE(return_type, name, p1, p2, p3)\
}

#define JS_ACCESS_4(return_type, name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) override {\
    v8toolkit::TypeList<t1, t2, t3, t4> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5(return_type, name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6(return_type, name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7(return_type, name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8, t9> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

#define JS_ACCESS_CONST(return_type, name)\
virtual return_type name() const override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_0_CONST(return_type, name)\
virtual return_type name() const override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(return_type, name)\
}


#define JS_ACCESS_1_CONST(return_type, name, t1)\
virtual return_type name(t1 p1) const override {\
    v8toolkit::TypeList<t1> typelist; \
    JS_ACCESS_CORE(return_type, name, p1)\
}

#define JS_ACCESS_2_CONST(return_type, name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) const override {\
    v8toolkit::TypeList<t1, t2> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2)\
}

#define JS_ACCESS_3_CONST(return_type, name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) const override {\
    v8toolkit::TypeList<t1, t2, t3> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3)\
}

#define JS_ACCESS_4_CONST(return_type, name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5_CONST(return_type, name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6_CONST(return_type, name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8, t9> typelist; \
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

// This can be extended to any number of parameters you need..

}

   


/**
Bidirectioal inheritance prototype chain looks like:

jswrapper instance 
jswrapper prototype
--- INSERT JAVASCRIPT CREATED PROTOTYPE HERE
base object prototype (**)
empty object
null


(**) This is where the `Base`-class implementations are found, but since they are virtual
   when called with a JSWrapper receiver object, the dynamic dispatch will actually call
   the JSWrapper JS_ACCESS function unless explicit static dispatch is used.

*/

