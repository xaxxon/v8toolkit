#pragma once

#include "v8_class_wrapper.h"


/**
* This file contains things needed to create "types" and objects in both C++ and javascript
*   and use them in either environment regardless of where they were created.  Since C++ 
*   types must exist at compile time, types created with these tools cannot create
*   actual C++ types, but instead allow behavior to be overridden on virtual functions
*
* See samples/bidirectional_sample.cpp for examples on how to use this library.
*/


namespace v8toolkit {

class BidirectionalException : std::exception {
    std::string reason;
public:
    BidirectionalException(const std::string & reason) : reason(reason) {}
    virtual const char * what() const noexcept {return reason.c_str();}
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
template<class T>
class JSWrapper {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Object> global_js_object;
	v8::Global<v8::FunctionTemplate> global_created_by;
    using BASE_TYPE = T;
    
    /**
    * It's easy to end up in infinite recursion where the JSWrapper object looks for a javascript implementation
    *   to call instead of calling the native C++ implementation, but finds its own JS_ACCESS function that its already in
    *   an proceeds to call itself.  This flag stops that from happening - at least in naive situations.   Marked as 
    *   mutable because it needs to be changed even from within const methods
    */ 
    mutable bool called_from_javascript = false;
public:
    JSWrapper(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::FunctionTemplate> created_by) :
        isolate(context->GetIsolate()), 
        global_context(v8::Global<v8::Context>(isolate, context)),
        global_js_object(v8::Global<v8::Object>(isolate, object)),
		global_created_by(v8::Global<v8::FunctionTemplate>(isolate, created_by))
		{}
};


/**
* Base class for a Factory that creates a bidirectional "type" by name.  The object
*   returned can be used as a class T regardless of whether it is a pure C++ type 
*   or if it has been extended in javascript
*/
template<class Base, class... ConstructorArgs>
class Factory {
public:
    virtual Base * operator()(ConstructorArgs... constructor_args) = 0;

    /**
    * Helper to quickly turn a Base type into another type if allowed
    */
    template<class U>
    U * as(ConstructorArgs...  constructor_args){
        // printf("Trying to cast a %s to a %s\n", typeid(Base).name(), typeid(U).name());
        auto result = this->operator()(constructor_args...);
        if (dynamic_cast<U*>(result)) {
            return static_cast<U*>(result);
        } else {
            throw BidirectionalException("Could not convert between types");
        }
    }
	
};


/**
* Returns a pure-C++ object of type Child which inherits from type Base.  It's Base type and ConstructorArgs... 
*   must match with the Factory it is associated with.
*/
template<class Base, class Child, class ... ConstructorArgs>
class CppFactory : public Factory<Base, ConstructorArgs...>{
public:
    virtual Base * operator()(ConstructorArgs... constructor_args) override 
    {
        // printf("CppFactory making a %s\n", typeid(Child).name());
        return new Child(constructor_args...);
    }
};


/**
* Returns a JavaScript-extended object inheriting from Base.  It's Base type and
*   ConstructorParameters must match up with the Factory class it is associated with
*/
template<class Base, class JSWrapperClass, class... ConstructorParameters>
class JSFactory : public Factory<Base, ConstructorParameters...> {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Function> global_javascript_function;

public:
    /**
    * Takes a context to use while calling a javascript_function that returns an object
    *   inheriting frmo JSWrapper
    */
    JSFactory(v8::Local<v8::Context> context, v8::Local<v8::Function> javascript_function) :
        isolate(context->GetIsolate()),
        global_context(v8::Global<v8::Context>(isolate, context)),
        global_javascript_function(v8::Global<v8::Function>(isolate, javascript_function))
    {}

    /**
    * Returns a C++ object inheriting from JSWrapper that wraps a newly created javascript object which
    *   extends the C++ functionality in javascript
    */
    Base * operator()(ConstructorParameters... constructor_parameters) {
        // printf("JSFactory making a %s\n", typeid(JSWrapperClass).name());
        
        return scoped_run(isolate, global_context, [&](auto isolate, auto context) {
            v8::Local<v8::Value> result;
            bool success = call_javascript_function(context, result, global_javascript_function.Get(isolate), context->Global(), std::tuple<ConstructorParameters...>(constructor_parameters...));
			assert(success);
			return V8ClassWrapper<Base>::get_instance(isolate).get_cpp_object(v8::Local<v8::Object>::Cast(result));
        });
    }
	
	/**
    * Creates a JavaScript helper function which is similar to Object.create() in that it creates an object implmenting templated type Base 
    *   which is ready to be extended in javascript and the prototype object containing the overrides.  See samples/bidirectional_sample.cpp for
    *   how to use it.  This function must be used when creating all "javascript subclassed objects" or they will not function properly.  
    *   Common use for this is to put it in your JSFactory JavaScript callback function.
    */
	static void add_subclass_function(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> object_template, const std::string & js_function_name)
	{
		v8toolkit::add_function(isolate, object_template, js_function_name.c_str(), [](const v8::FunctionCallbackInfo<v8::Value>& info)->void {
			auto isolate = info.GetIsolate();
			auto context = isolate->GetCurrentContext();

			// Grab the prototype for the subclass
			if (!info[0]->IsObject()) {
				isolate->ThrowException(v8::String::NewFromUtf8(isolate, "First parameter must be the subclass prototype object"));
				return;
			}
            auto subclass_prototype = v8::Local<v8::Object>::Cast(info[0]);

			// Create a new JavaScript object
			JSWrapperClass * new_cpp_object;
			auto & class_wrapper = V8ClassWrapper<JSWrapperClass>::get_instance(isolate);
			auto function_template = class_wrapper.get_function_template();
            auto new_js_object = function_template->GetFunction()->NewInstance();
                        			
			// Create the new C++ object - initialized with the JavaScript object
			// depth=1 to ParameterBuilder because the first parameter was already used (as the prototype)
			std::function<void(ConstructorParameters...)> constructor = [&](auto... args)->void{
				new_cpp_object = new JSWrapperClass(context, v8::Local<v8::Object>::Cast(new_js_object), function_template, args...);
			};
            
            using PB_TYPE = ParameterBuilder<1, decltype(constructor), decltype(constructor)>;
            if (!check_parameter_builder_parameter_count<PB_TYPE, 1>(info)) {
                // printf("add_subclass_function for %s got %d parameters but needed %d parameters\n", typeid(JSWrapperClass).name(), (int)info.Length()-1, (int)PB_TYPE::ARITY);
                isolate->ThrowException(v8::String::NewFromUtf8(isolate, "JSFactory::add_subclass_function constructor parameter count mismatch"));
                return;
            }
            
            
			PB_TYPE()(constructor, info);
			
			// now initialize the JavaScript object with the C++ object (circularly)
			// TODO: This circular reference may cause the GC to never destroy these objects
			class_wrapper.template initialize_new_js_object<DestructorBehavior_Delete<JSWrapperClass>>(isolate, new_js_object, new_cpp_object);

			// Set the prototypes appropriately  object -> subclass prototype (passed in) -> JSWrapper prototype -> base type prototype (via set_parent_type)
            (void)subclass_prototype->SetPrototype(context, new_js_object->GetPrototype());
            (void)new_js_object->SetPrototype(context, subclass_prototype);
            
			info.GetReturnValue().Set(new_js_object);
		});
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
        return this->BASE_TYPE::name( __VA_ARGS__ ); \
    } \
	if(JS_ACCESS_CORE_DEBUG) printf("IN JS_ACCESS_CORE\n");\
    auto parameter_tuple = std::make_tuple( __VA_ARGS__ );\
    v8toolkit::CastToNative<std::remove_reference<ReturnType>::type> cast_to_native;\
    return v8toolkit::scoped_run(isolate, global_context, [&](auto isolate, auto context){ \
      auto js_object = global_js_object.Get(isolate); \
        v8::TryCatch tc(isolate); \
        auto jsfunction = v8toolkit::get_key_as<v8::Function>(context, js_object, #name); \
        v8::Local<v8::Value> result; \
        this->called_from_javascript = true; \
        (void) v8toolkit::call_javascript_function(context, result, jsfunction, js_object, parameter_tuple); \
        this->called_from_javascript = false; \
        return cast_to_native(isolate, result); \
    });

// defines a JS_ACCESS function for a method taking no parameters
#define JS_ACCESS(return_type, name)\
virtual return_type name() override {\
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_1(return_type, name, t1)\
virtual return_type name(t1 p1) override {\
    JS_ACCESS_CORE(return_type, name, p1)\
}

#define JS_ACCESS_2(return_type, name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2)\
}

#define JS_ACCESS_3(return_type, name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3)\
}

#define JS_ACCESS_4(return_type, name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5(return_type, name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6(return_type, name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7(return_type, name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

#define JS_ACCESS_CONST(return_type, name)\
virtual return_type name() const override {\
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_1_CONST(return_type, name, t1)\
virtual return_type name(t1 p1) const override {\
    JS_ACCESS_CORE(return_type, name, p1)\
}

#define JS_ACCESS_2_CONST(return_type, name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2)\
}

#define JS_ACCESS_3_CONST(return_type, name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3)\
}

#define JS_ACCESS_4_CONST(return_type, name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5_CONST(return_type, name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6_CONST(return_type, name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9_CONST(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) const override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

// This can be extended to any number of parameters you need..

};

   


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

