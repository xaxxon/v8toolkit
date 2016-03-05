#pragma once

#include "v8toolkit.h"

namespace v8toolkit {

class BidirectionalException : std::exception {
    std::string reason;
public:
    BidirectionalException(const std::string & reason) : reason(reason) {}
    virtual const char * what() const noexcept {return reason.c_str();}
};

/**
* Type to inherit from for classes wrapping javascript objects wrapping c++ interfaces
* Example: class MyClass{};  class JSMyClass : public MyClass, public JSWrapper {};
*   Now, JSMyClass can be used as a MyClass, but will intercept calls to its methods
*   and attempt to use the javascript object to fulfill them, falling back to the
*   base class methods when necessary
*/
template<class T>
class JSWrapper {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Object> global_js_object;
    using BASE_TYPE = T;
public:
    JSWrapper(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : 
        isolate(context->GetIsolate()), 
        global_context(v8::Global<v8::Context>(isolate, context)),
        global_js_object(v8::Global<v8::Object>(isolate, object)) {}
};


template<class T, class... ConstructorArgs>
class Factory {
public:
    virtual T * operator()(ConstructorArgs... constructor_args) = 0;
    
    template<class U>
    U * as(ConstructorArgs...  constructor_args){
        auto result = this->operator()(constructor_args...);
        if (dynamic_cast<U*>(result)) {
            return static_cast<U*>(result);
        } else {
            throw BidirectionalException("Could not convert between types");
        }
    }
    
};

// Creates an instance of a class and returns a pointer to it
template<class Child, class Base, class ... ConstructorArgs>
class CppFactory : public Factory<Base, ConstructorArgs...>{
public:
    virtual Base * operator()(ConstructorArgs... constructor_args) override {return new Child(constructor_args...);}
};


// The first two parameters of the constructor for JSWrapperClass must be
//   a context and javascript object, then any other constructor parameters
template<class RealClass, class JSWrapperClass, class... ConstructorParameters>
class JSFactory : public Factory<RealClass, ConstructorParameters...> {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Function> global_javascript_function;

public:
    JSFactory(v8::Isolate * isolate, v8::Local<v8::Function> javascript_function) : 
        isolate(isolate),
        global_context(v8::Global<v8::Context>(isolate, isolate->GetCurrentContext())),
        global_javascript_function(v8::Global<v8::Function>(isolate, javascript_function))
        {
            assert(this->isolate);
            assert(this->isolate->InContext());
        }
    
    RealClass * operator()(ConstructorParameters... constructor_parameters) {
        return scoped_run(isolate, global_context, [&](auto isolate, auto context) {
            v8::Local<v8::Value> result;
            bool success = call_javascript_function(context, result, global_javascript_function.Get(isolate), context->Global(), std::tuple<ConstructorParameters...>(constructor_parameters...));
            assert(success);
            assert(result->IsObject());
            return new JSWrapperClass(context, v8::Local<v8::Object>::Cast(result), constructor_parameters...);
        });
    }
};


// Builds the body of a JS_ACCESS function 
// Takes the return type of the function, the name of the function, and a list of input variable names, if any
#define JS_ACCESS_CORE(ReturnType, name, ...) \
    auto parameter_tuple = std::make_tuple( __VA_ARGS__ );\
    v8toolkit::CastToNative<std::remove_reference<ReturnType>::type> cast_to_native;\
    return v8toolkit::scoped_run(isolate, global_context, [&](auto isolate, auto context){ \
        v8::TryCatch tc(isolate); \
        auto js_object = global_js_object.Get(isolate); \
        auto maybe_function_value = js_object->Get(context, v8::String::NewFromUtf8(isolate, #name)); \
        if(!maybe_function_value.IsEmpty()) { \
            /*printf("Got value when looking up %s\n", #name); */\
            if(!maybe_function_value.ToLocalChecked()->IsUndefined()) { \
                /*printf("..and the value was defined\n");*/ \
                auto jsfunction = v8::Local<v8::Function>::Cast(maybe_function_value.ToLocalChecked()); \
                if(!jsfunction.IsEmpty()) { \
                    /*printf("Calling the javascript function\n");*/ \
                    v8::Local<v8::Value> result; \
                    (void) v8toolkit::call_javascript_function(context, result, jsfunction, js_object, parameter_tuple); \
                    return cast_to_native(isolate, result); \
        }}} \
            /*printf("Falling back to C++ implementation\n");*/ \
        return this->std::remove_pointer<decltype(this)>::type::BASE_TYPE::name( __VA_ARGS__ ); \
    });


// defines a JS_ACCESS function for a method taking no parameters
//TODO: add const versions
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

// This can be extended to any number of parameters you need..

};

   
