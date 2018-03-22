#pragma once

#include <string>

#define V8TOOLKIT_BIDIRECTIONAL_ENABLED
#include "v8_class_wrapper.h"
#include "v8helpers.h"
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
    v8::Global<v8::Object> global_js_object;

    /**
    * It's easy to end up in infinite recursion where the JSWrapper object looks for a javascript implementation
    *   to call instead of calling the native C++ implementation, but finds its own JS_ACCESS function that its already in
    *   an proceeds to call itself.  This flag stops that from happening - at least in naive situations.   Marked as
    *   mutable because it needs to be changed even from within const methods
    */
    mutable bool called_from_javascript = false;

protected:
    JSWrapperBase(v8::Local<v8::Object> object) :
        global_js_object(v8::Global<v8::Object>(v8::Isolate::GetCurrent(), object))
    {}

public:
    v8::Local<v8::Object> get_javascript_object() const { 
        if (global_js_object.IsEmpty()) {
            return {};
        } else {
            return global_js_object.Get(v8::Isolate::GetCurrent());
        }
    }
};


/**
* C++ types to be extended in javascript must inherit from this class.
* Example: class MyClass{};  class JSMyClass : public MyClass, public JSWrapper<MyClass> {};
*   Now, JSMyClass can be used as a MyClass, but will intercept calls to its methods
*   and attempt to use the javascript object to fulfill them, falling back to the
*   C++ class methods of MyClass when necessary
* Any class inheriting from this (e.g. JSMyClass) must have the first two parameters of its constructor
*   be a v8::Local<v8::Context>, v8::Local<v8::Object>
*/
template<class Base>
class JSWrapper : public JSWrapperBase {
protected:
    using BASE_TYPE=Base; // used by JS_ACCESS macros
    
public:
    JSWrapper(Base * object) :
        JSWrapperBase(v8toolkit::V8ClassWrapper<Base>::get_instance(v8::Isolate::GetCurrent()).
            wrap_existing_cpp_object(v8::Isolate::GetCurrent()->GetCurrentContext(),
                                     object,
                                     *v8toolkit::V8ClassWrapper<Base>::get_instance(v8::Isolate::GetCurrent()).destructor_behavior_leave_alone,  // leave_alone may be wrong 
                                     true)
        )
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
struct CastToJS<T, std::enable_if_t<xl::is_template_for_v<v8toolkit::JSWrapper, T>>> {
    v8::Local<v8::Value> operator()(v8::Isolate *isolate, T const & js_wrapper) {
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
template<typename Base, // The common base class type of object to be returned
    typename,
    typename, // List of parameters that may change per-instance created
    typename Deleter> 
class Factory {
    static_assert(always_false_v<Base>, "This shouldn't ever be instantiated, only the specialization - check your types");
};


template<typename Base, typename... FixedArgs, typename... ConstructorArgs, typename Deleter>
class Factory<Base, TypeList<FixedArgs...>, TypeList<ConstructorArgs...>, Deleter>
{
public:
    using ThisFactoryType = Factory<Base, TypeList<FixedArgs...>, TypeList<ConstructorArgs...>, Deleter>;
    using base_type = Base;
    using js_wrapper = JSWrapper<Base>;
    
//     Factory(ParentTs&&... parent_ts) : ParentType(std::forward<ParentTs>(parent_ts)...)
//     {}
    Factory() = default;
    Factory(const Factory &) = delete;
    Factory(Factory &&) = default;
    Factory & operator=(const Factory &) = delete;
    Factory & operator=(Factory &&) = default;

    virtual ~Factory(){}

    /**
     * Returns a pointer to a new object inheriting from type Base
     */
    virtual Base * operator()(FixedArgs... fixed_args, ConstructorArgs... constructor_args) const = 0;
    virtual v8::Local<v8::Object> get_prototype() const = 0;



    Base * create(FixedArgs... fixed_args, ConstructorArgs... constructor_args) const {
        return this->operator()(std::forward<ConstructorArgs>(constructor_args)...);
    }

//    /**
//     * Returns a unique_ptr to a new object inheriting from type Base
//     */
//    template <typename U = Base>
//    std::unique_ptr<U, Deleter> get_unique(ConstructorArgs&&... args) const {
//
//	    // call operator() on the factory and put the results in a unique pointer
//        return std::unique_ptr<U, Deleter>((*this)(std::forward<ConstructorArgs>(args)...)); 
//    }

//    /**
//     * Helper to quickly turn a Base type into another type if allowed
//     */
//    template<typename U, typename... Args>
//    U * as(Args&&...  args) const {
//        // printf("Trying to cast a %s to a %s\n", typeid(Base).name(), typeid(U).name());
//        auto result = this->operator()(std::forward<Args>(args)...);
//        if (auto cast_result = dynamic_cast<U*>(result)) {
//            return cast_result;
//        } else {
//            throw BidirectionalException("Could not convert between types");
//        }
//    }


//    template<typename... Ts>
};



/**
* Returns a pure-C++ object of type Child which inherits from type Base.  It's Base type and ConstructorArgs... 
*   must match with the Factory it is associated with.  You can have it inherit from a type that inherits from v8toolkit::Factory
*   but v8toolkit::Factory must be in the inheritance chain somewhere
*/
template<typename Base,
    typename Derived,
    typename FixedParamsTypeList, // parameters to be sent to the constructor that are known at factory creation time
    typename ExternalTypeList,
    auto constructor_function = nullptr,
    typename Deleter = std::default_delete<Base> > 
class CppFactory;


// if the constructor wants a reference to the factory, automatically pass it in
template<
    typename Base, // interface being implemented
    typename Derived, // object type being created
    typename... FixedParams,
    typename... ExternalConstructorParams,
    auto constructor_function,
    typename Deleter> 
class CppFactory<
    Base,
    Derived,
    TypeList<FixedParams...>,
    TypeList<ExternalConstructorParams...>,
    constructor_function,
    Deleter> : public Factory<Base, TypeList<FixedParams...>, TypeList<ExternalConstructorParams...>, Deleter>
{
    

public:
    using ThisFactoryType = CppFactory<Base, TypeList<FixedParams...>, TypeList<ExternalConstructorParams...>, Deleter>;
    using FixedTypeList = TypeList<FixedParams...>;
    using FactoryBase = Factory<Base, TypeList<FixedParams...>, TypeList<ExternalConstructorParams...>, Deleter>;

    CppFactory()
    {}
    
    CppFactory(const CppFactory &) = delete;
    CppFactory(CppFactory &&) = default;
    CppFactory &operator=(const CppFactory &) = delete;
    CppFactory &operator=(CppFactory &&) = default;



    v8::Local<v8::Object> get_prototype() const override {
        v8::Isolate * isolate = v8::Isolate::GetCurrent();
        
        // need to get the prototype from the FunctionTemplate of the wrapped type
        return V8ClassWrapper<Derived>::get_instance(isolate).get_function_template()->GetFunction()->NewInstance();
    }

    /**
     * Don't call this directly, only create objects through the Factory interface
     * @param fixed_args 
     * @param constructor_args 
     * @return 
     */
    Base * operator()(FixedParams... fixed_args, ExternalConstructorParams... constructor_args) const override {
        if constexpr(constructor_function != nullptr) {
            return constructor_function(fixed_args..., constructor_args...);
        } else {
            return new Derived(fixed_args..., constructor_args...);
        }
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
    typename Base,
    typename JSWrapperClass,
    typename Internal,
    typename External,
    typename Deleter = std::default_delete<Base>>
class JSFactory;


// Begin real specialization
template<
	typename Base,
    typename JSWrapperClass,
    typename... FixedParams,
    typename... ExternalConstructorParams,
    typename Deleter>
class JSFactory<
    Base,
	JSWrapperClass,
	TypeList<FixedParams...>,
	TypeList<ExternalConstructorParams...>,
    Deleter> : public Factory<Base, TypeList<FixedParams...>, TypeList<ExternalConstructorParams...>, Deleter>
{
public:
    using FixedTypeList = TypeList<FixedParams...>;
    using ExternalTypeList = TypeList<ExternalConstructorParams...>;
    using FactoryBase = Factory<Base, FixedTypeList, ExternalTypeList, Deleter>;

protected:

    
    static_assert(std::is_polymorphic_v<JSWrapperClass>, "Wrapper class must be polymorphic - that's how function interception works");
    static_assert(std::is_base_of_v<Base, JSWrapperClass>, "Base type must be a base type of JSWrapperClass");
//    static_assert(std::is_base_of_v<JSWrapper<Base>, JSWrapperClass>, "JSWrapper<Base> must be a base type of JSWrapperClass");
    
	using ThisFactoryType = JSFactory<Base, JSWrapperClass, FixedTypeList, ExternalTypeList, Deleter>;


    
    // Create base portion of new object using wrapped type
    
    // javascript callback for creating properties only on the new type
    v8::Global<v8::Object> prototype;
    v8::Global<v8::Function> constructor;
    FactoryBase const * base_factory;


    /**
     * Create a JSFactory from pure javascript.  
     * JavaScript parameters need to be:
     * - Factory object of parent type
     * - Prototype
     * - Constructor function
     * - Internal parameters sent to every object's constructor
     */
    template<int starting_info_index, std::size_t... Is>
    static std::unique_ptr<FactoryBase> _create_factory_from_javascript(const v8::FunctionCallbackInfo<v8::Value> & info, std::index_sequence<Is...>) {

        auto isolate = info.GetIsolate();

        // Check that the call contains parameters for the base_factory, prototype, constructor, and each internal constructor parameter
        constexpr std::size_t parameter_count = starting_info_index + 3 + sizeof...(ExternalConstructorParams);
        if (info.Length() != parameter_count) {
            throw InvalidCallException(fmt::format("Wrong number of parameters to create new factory - needs {}, got {}", parameter_count, info.Length()));
        }

        // internal constructor params start 2 after the starting_info_index 
        int internal_param_index = starting_info_index + 3; // skip the base factory, prototype object and object constructor callback as well
        std::vector<std::unique_ptr<v8toolkit::StuffBase>> stuff;

       
        return std::make_unique<JSFactory>(
            CastToNative<FactoryBase&>()(isolate, info[starting_info_index + 0]),   // base factory        
            info[starting_info_index + 1]->ToObject(),                              // prototype
            v8::Local<v8::Function>::Cast(info[starting_info_index + 2]),           // constructor 
            ParameterBuilder<FixedParams>()(info, internal_param_index, stuff)...); // fixed params
    }

    
    v8::Local<v8::Function> get_constructor() const {
        return this->constructor.Get(v8::Isolate::GetCurrent());
    }

    

public:

    /**
     * Takes a context to use while calling a javascript_function that returns an object
     *   inheriting from JSWrapper
     */
    JSFactory(FactoryBase const & input_base_factory, 
              v8::Local<v8::Object> prototype,
              v8::Local<v8::Function> constructor) :
        prototype(v8::Global<v8::Object>(v8::Isolate::GetCurrent(), prototype)),
        constructor(v8::Global<v8::Function>(v8::Isolate::GetCurrent(), constructor))
    {
        assert(!prototype.IsEmpty());
        assert(!this->prototype.IsEmpty());
        
        // if base_factory is a CppFactory, then ignore it because the cpp object
        //   is created as part of the JSWrapperClass
        base_factory = dynamic_cast<ThisFactoryType const *>(&input_base_factory);

        auto isolate = v8::Isolate::GetCurrent();



        if (this->base_factory) {


            prototype->SetPrototype(this->base_factory->get_prototype());
        } else {
            auto base_factory_constructor = V8ClassWrapper<Base>::get_instance(isolate).get_function_template()->GetFunction();
            auto base_factory_prototype = base_factory_constructor->NewInstance();
            prototype->SetPrototype(base_factory_prototype);

        }
        constructor->SetPrototype(prototype);

    }

    ~JSFactory(){
//        printf("Deleting JSFactory object at %p\n", (void*)this);
    }

    
    JSWrapperClass * operator()(FixedParams... fixed_params, 
                                ExternalConstructorParams... constructor_params) const override {


        auto isolate = v8::Isolate::GetCurrent();
        auto context = isolate->GetCurrentContext();

        JSWrapperClass * const wrapper_class = [&]() {

            // at the least-derived JS factory, create the JSWrapperClass object
            if (this->base_factory == nullptr) {
                return new JSWrapperClass(fixed_params..., constructor_params...);
            } else {
                return static_cast<JSWrapperClass *>(this->base_factory->operator()(fixed_params..., constructor_params...));
            }
        }();

       
        wrapper_class->get_javascript_object()->SetPrototype(this->get_prototype());

        call_javascript_function_with_vars(context,
                                           this->get_constructor(),
                                           wrapper_class->get_javascript_object(),
                                           TypeList<ExternalConstructorParams...>(),
                                           constructor_params...);
        
        
        return wrapper_class;
    }


    template<int starting_info_index>
    static std::unique_ptr<FactoryBase> create_factory_from_javascript(const v8::FunctionCallbackInfo<v8::Value> & info) {
	    return _create_factory_from_javascript<starting_info_index>(info, std::index_sequence_for<FixedParams...>());
    }

    
    v8::Local<v8::Object> get_prototype() const override {
        assert(!this->prototype.IsEmpty());
        return this->prototype.Get(v8::Isolate::GetCurrent());
    }
};


template<typename Base, typename FixedParamTypeList, typename ConstructorParamTypeList, typename Deleter = std::default_delete<Base>>
class ConcreteFactory {
    static_assert(always_false_v<Base>, "Only the specialization should ever be instantiated - check your types");
};


template<typename Base, typename... FixedParams, typename... ConstructorParams, typename Deleter>
class ConcreteFactory<Base, TypeList<FixedParams...>, TypeList<ConstructorParams...>, Deleter> {
    using FixedParamTypeList = TypeList<FixedParams...>;
    using ConstructorParamTypeList = TypeList<ConstructorParams...>;
    using ThisFactory = ConcreteFactory<Base, FixedParamTypeList, ConstructorParamTypeList, Deleter>;

private:

    using FactoryType = Factory<Base, FixedParamTypeList, ConstructorParamTypeList, Deleter>;
    std::unique_ptr<FactoryType> factory;
    std::tuple<FixedParams...> fixed_params;

    template<size_t... Is>
    Base * create_helper(ConstructorParams... constructor_params, std::index_sequence<Is...>) const {
        return factory->operator()(std::get<Is>(fixed_params)..., constructor_params...);
    }

public:

    template<typename Derived, Derived*(*constructor_function)(FixedParams..., ConstructorParams...) = nullptr>
    struct CppFactoryInfo{};
    
    template<typename JSWrapperClass>
    struct JSFactoryInfo{
        ThisFactory const * base_concrete_factory;
        v8::Local<v8::Object> prototype;
        v8::Local<v8::Function> constructor;
        
        JSFactoryInfo(ThisFactory const * base_concrete_factory,
                      v8::Local<v8::Object> prototype,
                      v8::Local<v8::Function> constructor) :
            base_concrete_factory(base_concrete_factory),
            prototype(prototype),
            constructor(constructor)
        {}
    };
    


    template<typename Derived, Derived*(*constructor_function)(FixedParams..., ConstructorParams...)>
    ConcreteFactory(CppFactoryInfo<Derived, constructor_function> const & cpp_factory_info, FixedParams... params) :
        factory(std::make_unique<CppFactory<Base, Derived, FixedParamTypeList, ConstructorParamTypeList, constructor_function, Deleter>>()), 
        fixed_params(params...)
    {}


    template<typename JSWrapperType>
    ConcreteFactory(JSFactoryInfo<JSWrapperType> const & js_factory_info, FixedParams... params) :
        factory(std::make_unique<JSFactory<Base, JSWrapperType, FixedParamTypeList, ConstructorParamTypeList, Deleter>>(
            *js_factory_info.base_concrete_factory->factory,
            js_factory_info.prototype,
            js_factory_info.constructor
        )),
        fixed_params(params...)
    {}



    Base * operator()(ConstructorParams... constructor_params) const {
        return this->create(constructor_params...);
    }
    
    Base * create(ConstructorParams... constructor_params) const {
        return create_helper(constructor_params..., std::index_sequence_for<FixedParams...>());
    }

    static void wrap_factory(v8::Isolate * isolate) {
        V8ClassWrapper<ThisFactory> & wrapper = V8ClassWrapper<ThisFactory>::get_instance(isolate);
        if (!wrapper.is_finalized()) {
            wrapper.add_method("create", &ThisFactory::create);
            wrapper.finalize();
        }
    }

};

template<typename T>
struct is_wrapped_type<T, std::enable_if_t<xl::is_template_for_v<ConcreteFactory, T>>> : std::true_type
{};



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
#define JS_ACCESS_CORE(ReturnType, name, js_name, ...) \
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
	if(JS_ACCESS_CORE_DEBUG) printf("IN JS_ACCESS_CORE for %s, not calling native code\n", #js_name); \
    /*auto parameter_tuple = std::make_tuple( __VA_ARGS__ ); */ \
   /* auto parameter_tuple = make_tuple_for_variables(__VA_ARGS__); */ \
    v8toolkit::CastToNative<std::remove_reference<ReturnType>::type> cast_to_native; \
    auto isolate = v8::Isolate::GetCurrent(); \
    auto context = isolate->GetCurrentContext(); \
    CONTEXT_SCOPED_RUN(context); \
    auto js_object = global_js_object.Get(isolate); \
    v8::Local<v8::Function> js_function; \
    v8::TryCatch tc(isolate); \
    try { \
        js_function = v8toolkit::get_key_as<v8::Function>(context, js_object, #js_name); \
    } catch (...) {assert(((void)"method probably not added to wrapped parent type", false) == true); throw;} \
    this->called_from_javascript = true; \
    auto result = v8toolkit::call_javascript_function_with_vars(context, js_function, js_object, typelist, ##__VA_ARGS__); \
    this->called_from_javascript = false; \
    return cast_to_native(isolate, result);


// defines a JS_ACCESS function for a method taking no parameters
#define JS_ACCESS(return_type, name, js_name)\
virtual return_type name() override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name)\
}

#define JS_ACCESS_0(return_type, name, js_name)\
virtual return_type name() override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name)\
}

#define JS_ACCESS_1(return_type, name, js_name, t1)\
virtual return_type name(t1 p1) override {\
    v8toolkit::TypeList<t1> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1)\
}

#define JS_ACCESS_2(return_type, name, js_name, t1, t2) \
virtual return_type name(t1 p1, t2 p2) override { \
    v8toolkit::TypeList<t1, t2> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2) \
}

#define JS_ACCESS_3(return_type, name, js_name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) override { \
    v8toolkit::TypeList<t1, t2, t3> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3) \
}

#define JS_ACCESS_4(return_type, name, js_name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) override {\
    v8toolkit::TypeList<t1, t2, t3, t4> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5(return_type, name, js_name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6(return_type, name, js_name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8, t9> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

#define JS_ACCESS_CONST(return_type, name, js_name)\
virtual return_type name() const override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name)\
}

#define JS_ACCESS_0_CONST(return_type, name, js_name)\
virtual return_type name() const override {\
    v8toolkit::TypeList<> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name)\
}


#define JS_ACCESS_1_CONST(return_type, name, js_name, t1)\
virtual return_type name(t1 p1) const override {\
    v8toolkit::TypeList<t1> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1)\
}

#define JS_ACCESS_2_CONST(return_type, name, js_name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) const override {\
    v8toolkit::TypeList<t1, t2> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2)\
}

#define JS_ACCESS_3_CONST(return_type, name, js_name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) const override {\
    v8toolkit::TypeList<t1, t2, t3> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3)\
}

#define JS_ACCESS_4_CONST(return_type, name, js_name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5_CONST(return_type, name, js_name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6_CONST(return_type, name, js_name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7_CONST(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8_CONST(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9_CONST(return_type, name, js_name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) const override {\
    v8toolkit::TypeList<t1, t2, t3, t4, t5, t6, t7, t8, t9> typelist; \
    JS_ACCESS_CORE(V8TOOLKIT_MACRO_TYPE(return_type), name, js_name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
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

