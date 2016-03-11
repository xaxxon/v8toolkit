#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>

#include <string.h>

#include "v8helpers.h"
#include "casts.hpp"

#include <dirent.h>


#define V8_TOOLKIT_DEBUG false


/** TODO LIST
*
* Rename project to v8toolkit everywhere including github
* All includes should be included as #include "v8toolkit/FILENAME.h" instead of just #include "FILENAME.h"
* Rename javascript.h file to something much better
* Rename v8toolkit.h contents to something else so it can be included directory but using a different name
* Change *Helper classes to just their base name.  They're in a namespace, so it shouldn't be too confusing.
* Including "v8toolkit/v8toolkit.h" should include everything, but specific includes should also work for
*   v8helpers.h, <new name for v8toolkit.h>, <new name for javascript.h>
*/






namespace v8toolkit {
    
/**
* Takes a v8::Value and prints it out in a json-like form (but includes non-json types like function)
*
* Good for looking at the contents of a value and also used for printobj() method added by add_print
*/
std::string stringify_value(v8::Isolate * isolate, const v8::Local<v8::Value> & value, bool toplevel=true);


/**
* General purpose exception for invalid uses of the v8toolkit API
*/
class InvalidCallException : public std::exception {
private:
    std::string message;
    
public:
  InvalidCallException(std::string message) : message(message) {}
  virtual const char * what() const noexcept override {return message.c_str();}
};



/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
*/
template<class T, 
    class R = decltype(std::declval<T>()()),
    decltype(std::declval<T>()(), 1) = 1>
R scoped_run(v8::Isolate * isolate, T callable)
{
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    if (isolate->InContext()) {
        auto context = isolate->GetCurrentContext();
        v8::Context::Scope context_scope(context);
        return callable();
    } else {
        return callable();
    }
}


/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
* The isolate will be passed to the callable
*/
template<class T, 
         class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
         decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
R scoped_run(v8::Isolate * isolate, T callable)
{   
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    return callable(isolate);
}

/**
* Helper function to run the callable inside contexts.
* If the isolate is currently inside a context, it will use that context automatically
*   otherwise no context::scope will be created
* This version requires the isolate is currently in a context
* The isolate and context will be passed to the callable
*/
template<class T, 
         class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
         decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
R scoped_run(v8::Isolate * isolate, T callable)
{   
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    if (isolate->InContext()) {
        auto context = isolate->GetCurrentContext();
        v8::Context::Scope context_scope(context);
        return callable(isolate, context);
    } else {
        throw InvalidCallException("Isolate not currently in a context, but callable expects a context.");
    }
}



// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
*/
template<class T, 
         class R = decltype(std::declval<T>()()),
         decltype(std::declval<T>()(), 1) = 1>
R scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable)
{
    
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable();
}


// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
* The isolate will be passed to the callable
*/
template<class T, 
         class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
         decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
R scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable)
{   
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable(isolate);
}

// TODO: Probably don't need to take both an isolate and a local<context> - you can get isolate from a local<context> (but not a global one)
/**
* Helper function to run the callable inside contexts.
* This version is good when the isolate isn't currently within a context but a context
*   has been created to be used
* The isolate and context will be passed to the callable
*/
template<class T, 
         class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
         decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
R scoped_run(v8::Isolate * isolate, v8::Local<v8::Context> context, T callable)
{   
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope context_scope(context);

    return callable(isolate, context);
}
    

// Same as the ones above, but this one takes a global context for convenience
// Isolate is required since a Local<Context> cannot be created without creating a locker
//   and handlescope which require an isolate to create
template<class T,
class R = decltype(scoped_run(std::declval<v8::Isolate*>(), std::declval<T>()))>
R scoped_run(v8::Isolate * isolate, const v8::Global<v8::Context> & context, T callable) {
    v8::Locker l(isolate);
    v8::HandleScope hs(isolate);
    auto local_context = context.Get(isolate);
    return scoped_run(isolate, local_context, callable);
}
    

/**
* Functor to call a given std::function and, if it has a non-null return value, return its value back to javascript
* Mostly for v8toolkit internal use
*/
template<class T>
struct CallCallable{};

/**
* specialization for functions with non-void return types so the value is sent back to javascript
*/
template<class R, typename ... Args>
struct CallCallable<std::function<R(Args...)>> {
    void operator()(std::function<R(Args...)> callable, 
                    const v8::FunctionCallbackInfo<v8::Value> & info, Args... args) {
        info.GetReturnValue().Set(v8toolkit::CastToJS<R>()(info.GetIsolate(), callable(args...)));
        if (V8_TOOLKIT_DEBUG) printf("Just set returnvalue in CallCallable for type %s\n", typeid(R).name());
    }
};

/**
* specialization for functions with a void return type and there is nothing to be sent back to javascript
*/
template<typename ... Args>
struct CallCallable<std::function<void(Args...)>> {
    void operator()(std::function<void(Args...)> callable, 
                    const v8::FunctionCallbackInfo<v8::Value> & info, Args... args) {
        callable(args...);
    }
};



/**
* Class for turning a function parameter list into a parameter pack useful for calling the function
*/
template<int depth, typename T, typename U, class = void> 
struct ParameterBuilder;


/**
* Specialization for when there are no parameters left to process, so call the function now
*/  
template<int depth, typename FUNCTION_TYPE, typename RET>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET()>> {
    // the final class in the call chain stores the actual method to be called

    enum {DEPTH=depth, ARITY=0};
    
    // This call method actually calls the function with the specified object and the
    //   parameter pack that was built up via the chain of calls between templated types
    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
        // use CallCallable to differentiate between void and non-void return types
        CallCallable<FUNCTION_TYPE>()(function, info, ts...);
    }
};


/**
* specialization that strips off the first remaining parameter off the function type, stores that and then
*   inherits from another instance that either strips the next one off, or if none remaining, actually calls
*   the function
* The function type is specified twice because the first is actually used by the final specialization to hold the 
*   function type while the second one has its input parameter list stripped off one at a time to determine when
*   the inheritance chain ends
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename HEAD, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(HEAD,TAIL...)>, 
                        std::enable_if_t<!std::is_pointer<HEAD>::value || !std::is_fundamental< typename std::remove_pointer<HEAD>::type >::value > >: 
        public ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> {

    typedef ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
    enum {DEPTH = depth, ARITY=super::ARITY + 1};

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
        // printf("Parameter builder HEAD: %s\n", typeid(HEAD).name());
        this->super::operator()(function, info, ts..., CastToNative<typename std::remove_reference<HEAD>::type>()(info.GetIsolate(), info[depth])); 
    }
};
    
/**
* Specialization for function taking a char *
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(char *, TAIL...)>> : 
					      public ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
    
    typedef ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
    enum {DEPTH = depth, ARITY=super::ARITY+1};
    std::unique_ptr<char[]> buffer;
    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
      this->buffer = CastToNative<char *>()(info.GetIsolate(), info[depth]);
      this->super::operator()(function, info, ts..., buffer.get());
    }
};

/**
* Specialization for function taking a const char *
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(const char *, TAIL...)>> : 
					      public ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
    
    typedef ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
    enum {DEPTH = depth, ARITY=super::ARITY+1};
    std::unique_ptr<char[]> buffer;
    
    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
      this->buffer = CastToNative<const char *>()(info.GetIsolate(), info[depth]);
      this->super::operator()(function, info, ts..., buffer.get()); 
    }
};


/**
* Specialization that deals with pointers to primitive types by creating a holder that the address of can be passed along
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename HEAD, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(HEAD*, TAIL...)>, std::enable_if_t< std::is_fundamental<HEAD>::value >> :
    public ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> {

    typedef ParameterBuilder<depth+1, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
    enum {DEPTH = depth, ARITY=super::ARITY+1};
    
    // This variable's address will be passed into the function to be called
    HEAD element;

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
      this->element = CastToNative<HEAD>()(info.GetIsolate(), info[depth]);
      this->super::operator()(function, info, ts..., &this->element);
    }
};


 
/**
* specialization for functions that want to take a v8::FunctionCallbackInfo object in addition
*   to javascript-provided parameters.  depth parameter isn't incremented because this doesn't
*   eat one of the javascript parameter values
* Unlike the void(const v8::FunctionCallbackInfo<v8::Value>&) specialization, when this 
*   specialization is used, normal parameter passing and return value processing is still
*   done, but this parameter is injected directly, not taken from the parameter list from
*   javascript
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(const v8::FunctionCallbackInfo<v8::Value> &,TAIL...)>> : 
        public ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
            
    typedef ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>> super;
    enum {DEPTH = depth, ARITY=super::ARITY};

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
        this->super::operator()(function, info, ts..., info);
    }
};


/**
* Specialization for functions that want the isolate pointer (but not all the rest of the stuff
*   in the FunctionCallbackInfo for simplicity's sake)
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(v8::Isolate *, TAIL...)>> : 
        public ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
            
    using super = ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>>;
    enum {DEPTH = depth, ARITY=super::ARITY};

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
        this->super::operator()(function, info, ts..., info.GetIsolate()); 
    }
};


/**
* Specialization for a function that wants the context
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(v8::Local<v8::Context>, TAIL...)>> : 
        public ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
    using super = ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>>;
    enum {DEPTH = depth, ARITY=super::ARITY};

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
		auto context = info.GetIsolate()->GetCurrentContext();
        this->super::operator()(function, info, ts..., context);
    }
};


/**
* Specialization for function wanting the receiver JS object (object being created for constructors)
*/
template<int depth, typename FUNCTION_TYPE, typename RET, typename...TAIL>
struct ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(v8::Local<v8::Object>, TAIL...)>> : 
        public ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>> {
    using super = ParameterBuilder<depth, FUNCTION_TYPE, std::function<RET(TAIL...)>>;
    enum {DEPTH = depth, ARITY=super::ARITY};

    template<typename ... Ts>
    void operator()(FUNCTION_TYPE function, const v8::FunctionCallbackInfo<v8::Value> & info, Ts... ts) {
        this->super::operator()(function, info, ts..., info.This());
    }
};





/**
* Specialization to handle functions that want the javascript callback info directly
* Useful for things that want to handle multiple, untyped arguments in a custom way (like the print functions provided in this library)
* Any return value must be handled directly by the function itself by populating info parameter
*/
template<int depth, class T>
struct ParameterBuilder<depth, T, std::function<void(const v8::FunctionCallbackInfo<v8::Value>&)>>
{
    enum {ARITY=0};
    
    void operator()(std::function<void(const v8::FunctionCallbackInfo<v8::Value> &)> function, const v8::FunctionCallbackInfo<v8::Value> & info) {
        function(info);
    }
};



/**
* Creates a function template from a std::function
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, std::function<R(Args...)> f)
{
    auto copy = new std::function<R(Args...)>(f);
    return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();

        auto callable = *(std::function<R(Args...)>*)v8::External::Cast(*(info.Data()))->Value();

        using PB_TYPE = ParameterBuilder<0, std::function<R(Args...)>, std::function<R(Args...)>>;
        PB_TYPE pb;
        
        auto arity = PB_TYPE::ARITY;
        if(info.Length() < arity) {
            std::stringstream ss;
            ss << "Function called from javascript with insufficient parameters.  Requires " << arity << " provided " << info.Length();
            isolate->ThrowException(v8::String::NewFromUtf8(isolate, ss.str().c_str()));
            return;
        }
        std::exception_ptr exception_pointer;
        try {
            pb(callable, info);
        } catch (...) {
            auto anyptr_t = new Any<std::exception_ptr>( std::current_exception());
            
            // always put in the base ptr so you can cast to it safely and then use dynamic_cast to try to figure
            //   out what it really is
            isolate->ThrowException(v8::External::New(isolate, static_cast<AnyBase*>(anyptr_t)));
        }
        return;
    }, v8::External::New(isolate, (void*)copy));
}


/**
* Takes an arbitrary class method and returns a std::function wrapping it
*/
template<class R, class CLASS, class... Args>
std::function<R(Args...)> make_std_function_from_callable(R(CLASS::*f)(Args...) const, CLASS callable ) 
{
    return std::function<R(Args...)>(callable);
}


/**
* Creates a v8::FunctionTemplate for an arbitrary callable 
*/
template<class T>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, T callable) 
{
    return make_function_template(isolate, make_std_function_from_callable(&T::operator(), callable));
}



/**
* Creates a function template from a c-style function pointer
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, R(*f)(Args...))
{
    return make_function_template(isolate, std::function<R(Args...)>(f));
}


/**
* Helper to both create a function template from a std::function and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, std::function<R(Args...)> function) {
    object_template->Set(isolate, name, make_function_template(isolate, function));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class T>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T callable) {
    object_template->Set(isolate, name, make_function_template(isolate, callable));
}

/**
* Helper to both create a function template from an arbitrary function pointer and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, R(*function)(Args...)) {
    object_template->Set(isolate, name, make_function_template(isolate, function));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object allows adding a function to any object, including a context's global object.
*/

template<class T>
void add_function(const v8::Local<v8::Context> & context, const v8::Local<v8::Object> & object, const char * name, T callable) 
{
    auto isolate = context->GetIsolate();
    scoped_run(isolate, context, [&](){
        auto function_template = make_function_template(isolate, callable);
        auto function = function_template->GetFunction();
        (void)object->Set(context, v8::String::NewFromUtf8(isolate, name), function);
    });
}

/**
* Makes the given javascript value available to all objects created with the object_template as name.
* Often used to populate the object_template used to create v8::Context objects so the variable is available from 
*   all contexts created from that object_template
*/
void add_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, const v8::Local<v8::Data> value);


/**
* Makes the given javascript value available in the given object as name.
* Often used to add a variable to a specific context's global object
*/
void add_variable(const v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, const v8::Local<v8::Value> value);



/**
* add a function that directly handles the v8 callback data
* explicit function typing needed to coerce non-capturing lambdas into c-style function pointers
*/
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&));

template<int position, class Tuple>
struct TupleForEach;

/**
* Populates an array of v8::Values with the values of the tuple, casted by the tuple element's type 
*/
template<int position, class Tuple>
struct TupleForEach : public TupleForEach<position - 1, Tuple> {
    using super = TupleForEach<position - 1, Tuple>;
    void operator()(v8::Isolate * isolate, v8::Local<v8::Value> * params, const Tuple & tuple){
        constexpr int array_position = position - 1;
        params[array_position] = CastToJS<typename std::tuple_element<array_position, Tuple>::type>()(isolate, std::get<array_position>(tuple));
        super::operator()(isolate, params, tuple);
    }
};

/** 
* Base case for no remaining elements to parse (as determined by the position being 0)
*/
template<class Tuple>
struct TupleForEach<0, Tuple> {
  void operator()(v8::Isolate *, v8::Local<v8::Value> *, const Tuple &){}
};



/**
* Returns true on success with the result in the "result" parameter
*/
template<class TupleType = std::tuple<>>
bool call_javascript_function(const v8::Local<v8::Context> context,
                              v8::Local<v8::Value> & result,
                              const v8::Local<v8::Function> function,
                              const v8::Local<v8::Object> receiver,
                              const TupleType & tuple = {})
{
    constexpr int tuple_size = std::tuple_size<TupleType>::value;
    v8::Local<v8::Value> parameters[tuple_size];
    auto isolate = context->GetIsolate();
    TupleForEach<tuple_size, TupleType>()(isolate, parameters, tuple);
    
    v8::TryCatch tc(isolate);
    
    // printf("\n\n**** Call_javascript_function with receiver: %s\n", stringify_value(isolate, v8::Local<v8::Value>::Cast(receiver)).c_str());
    auto maybe_result = function->Call(context, receiver, tuple_size, parameters);
    if(tc.HasCaught() || maybe_result.IsEmpty()) {
        printf("error: %s or result was empty\n", *v8::String::Utf8Value(tc.Exception()));
        return false;
    }
    result = maybe_result.ToLocalChecked();
    return true;
}

/**
* Returns true on success with the result in the "result" parameter
*/
template<class TupleType = std::tuple<>>
bool call_javascript_function(const v8::Local<v8::Context> context,
                              v8::Local<v8::Value> & result,
                              const std::string & function_name,
                              const v8::Local<v8::Object> receiver,
                              const TupleType & tuple = {})
{
    auto maybe_value = receiver->Get(context, v8::String::NewFromUtf8(context->GetIsolate(),function_name.c_str()));
    if(maybe_value.IsEmpty()) {
        return false;
    }
    
    auto value = maybe_value.ToLocalChecked();
    if(!value->IsFunction()) {
        return false;
    }    
    
    bool success = call_javascript_function(context, result, v8::Local<v8::Function>::Cast(value), receiver, tuple);
    return success;
}


// helper for getting exposed variables
template<class VARIABLE_TYPE>
void _variable_getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    auto isolate = info.GetIsolate();
    VARIABLE_TYPE * variable = (VARIABLE_TYPE*)v8::External::Cast(*(info.Data()))->Value();
    info.GetReturnValue().Set(CastToJS<VARIABLE_TYPE>()(isolate, *variable));
}

// helper for setting exposed variables
template<class VARIABLE_TYPE>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info) 
{
    // using ResultType = decltype(CastToNative<VARIABLE_TYPE>()(info.GetIsolate(), value));
    // TODO: This doesnt work well with pointer types - we want to assign to the dereferenced version, most likely.
    *(VARIABLE_TYPE*)v8::External::Cast(*(info.Data()))->Value() = CastToNative<VARIABLE_TYPE>()(info.GetIsolate(), value);
}


/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads and writes to the variable
*/
template<class VARIABLE_TYPE>
void expose_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, VARIABLE_TYPE & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name), 
                                 _variable_getter<VARIABLE_TYPE>, 
                                 _variable_setter<VARIABLE_TYPE>, 
                                 v8::External::New(isolate, &variable));
}
/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads to the variable.  Writes are ignored.
* TODO: consider making writes errors (throw?)
*/
template<class VARIABLE_TYPE>
void expose_variable_readonly(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, VARIABLE_TYPE & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name), 
                                 _variable_getter<VARIABLE_TYPE>, 
                                 0, 
                                 v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read/write variable
* Often used to add the variable to a context's global object
*/
template<class VARIABLE_TYPE>
void expose_variable(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, VARIABLE_TYPE & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name), 
                        _variable_getter<VARIABLE_TYPE>, 
                        _variable_setter<VARIABLE_TYPE>, 
                        v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read-only variable (writes are ignored but are not errors)
* TODO: consider making them errors (throw?)
* Often used to add the variable to a context's global object
*/
template<class VARIABLE_TYPE>
void expose_variable_readonly(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, VARIABLE_TYPE & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name), _variable_getter<VARIABLE_TYPE>, 0, v8::External::New(isolate, &variable));
}


/**
* Takes a local and creates a weak global reference callback for it
* Useful for clearing out C++-allocated memory on javascript garbage collection of an associated javascript object
*/
template<class CALLBACK_FUNCTION>
void global_set_weak(v8::Isolate * isolate, const v8::Local<v8::Object> & javascript_object, CALLBACK_FUNCTION function)
{
    struct SetWeakCallbackData{
        SetWeakCallbackData(CALLBACK_FUNCTION function, v8::Isolate * isolate, const v8::Local<v8::Object> & javascript_object) : 
            function(function) {
                this->global.Reset(isolate, javascript_object);
        }
        CALLBACK_FUNCTION function;
        v8::Global<v8::Object> global;
    };
    
    auto callback_data = new SetWeakCallbackData(function, isolate, javascript_object);
    callback_data->global.template SetWeak<SetWeakCallbackData>(callback_data,
        [](const v8::WeakCallbackData<v8::Object, SetWeakCallbackData> & data){
            SetWeakCallbackData * callback_data = data.GetParameter();          
            callback_data->function();
            callback_data->global.Reset();
            delete callback_data;
        });
}





// takes a format string and some javascript objects and does a printf-style print using boost::format
// fills missing parameters with empty strings and prints any extra parameters with spaces between them
std::string _printf_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline);


/**
* Returns the values in a FunctionCallbackInfo object breaking out first-level arrays into their
*  contained values (but not subsequent arrays for no particular reason)
*/
std::vector<v8::Local<v8::Value>> get_all_values(const v8::FunctionCallbackInfo<v8::Value>& args, int depth = 1);



// prints out arguments with a space between them
std::string _print_helper(const v8::FunctionCallbackInfo<v8::Value>& args, bool append_newline);

/**
* Adds the print functions listed below to the given object_template (usually a v8::Context's global object)
* Optional callback function can be used to send the output to another source (defaults to stdout)
*
* call this to add a set of print* functions to whatever object template you pass in (probably the global one)
* print takes a single variable or an array and prints each value separated by spaces
*
* println same as print but automatically appends a newlines
*
* printf - Treats the first parameter as a format string.  
*          any additional values will be used to fill the format string.  If there are insufficient parameters
*          to fill the format, the empty string "" will be used.   Any extra parameters will be printed after
*          the filled format string separated by spaces
*
* printfln - same as printf but automatically appends a newline
*
* printobj - prints a bunch of information about an object - format highly susceptible to frequent change
*/
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> object_template, std::function<void(const std::string &)> = [](const std::string & s){printf("%s", s.c_str());} );

/**
* Adds an assert method that calls assert.h assert() on failure.  This is different than the add_assert() in javascript.h that throws an exception on failure
*   because if an exception is not caught before it reaches V8 execution, the program is terminated.  javascript.h *Helper classes automatically catch
*   and re-throw exceptions so it is safe to throw in that version, but not this one.  The error message resulting from throwing an exception
*   reaching code compiled without exception support is not easy to understand which is why a simple assert is preferable.
*/
void add_assert(v8::Isolate * isolate,  v8::Local<v8::ObjectTemplate> object_template);

// returns true if the two values are the same by value, including nested data structures
bool compare_contents(v8::Isolate * isolate, const v8::Local<v8::Value> & left, const v8::Local<v8::Value> & right);


/**
* Accepts an object and a method on that object to be called later via its operator()
* Does not require knowledge of how many parameters the method takes or any placeholder arguments
* Can be wrapped with a std::function
*/
template<class T, class U>
struct Bind{};

/** 
* Bind specialization for handling non-const class methods
*/
template<class CLASS_TYPE, class R, class... Args>  
struct Bind<CLASS_TYPE, R(CLASS_TYPE::*)(Args...)> {
    
    Bind(CLASS_TYPE & object, R(CLASS_TYPE::*method)(Args...) ) :
      object(object), method(method){}
      ~Bind(){}
    
    CLASS_TYPE & object;
    R(CLASS_TYPE::*method)(Args...);
    
    R operator()(Args... params){
        return (object.*method)(params...); 
    }
};


/** 
* Bind specialization for handling const class methods
*/
template<class CLASS_TYPE, class R, class... Args>  
struct Bind<CLASS_TYPE, R(CLASS_TYPE::*)(Args...) const> {
    
    Bind(CLASS_TYPE & object, R(CLASS_TYPE::*method)(Args...) const) :
      object(object), method(method){}
    
    CLASS_TYPE & object;
    R(CLASS_TYPE::*method)(Args...) const;
    
    R operator()(Args... params){
        return (object.*method)(params...); 
    }
};



/**
* Helper function to create a Bind object using type deduction and wrap it in a
* std::function object.
* This specialization is for handling non-const class methods
*/
template <class CLASS, class R, class... Args>
std::function<R(Args...)> bind(CLASS & object, R(CLASS::*method)(Args...))
{
    return std::function<R(Args...)>(Bind<CLASS, R(CLASS::*)(Args...)>(object, method));
}

/**
* Helper function to create a Bind object using type deduction and wrap it in a
* std::function object.
* This specialization is for handling const class methods
*/
template <class CLASS, class R, class... Args>
std::function<R(Args...)> bind(CLASS & object, R(CLASS::*method)(Args...) const)
{
    return std::function<R(Args...)>(Bind<CLASS, R(CLASS::*)(Args...) const>(object, method));
}




/**
* Example allocator code from V8 Docs
*/
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  inline virtual void* Allocate(size_t length) override {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  inline virtual void* AllocateUninitialized(size_t length) override { return malloc(length); }
  inline virtual void Free(void* data, size_t) override { free(data); }
};


/**
* If the filename `filename` exists, reeturns true and sets the last modificaiton time and contents
*   otherwise returns false
*/
bool get_file_contents(std::string filename, std::string & file_contents, time_t & file_modification_time);

/**
* same as longer version, just doesn't return modification time if it's not desired
*/
bool get_file_contents(std::string filename, std::string & file_contents);



/**
* adds 'require' method to javascript to emulate node require.
* Adds an self-referential "global" alias to the global object
* Must be run after the context is created so "global" can refer to the global object
*   (if this could be changed, it should be, but I don't know how to do it beforehand)
*/
void add_require(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & context, const std::vector<std::string> & paths);


/**
* adds "module_list()" to javascript to require a dictionary of module path+names to exported objects
*   currently required into the specified isolate
*/
void add_module_list(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template);


/**
* Attempts to load the specified module name from the given paths (in order).
*   Returns the exported object from the module.
* Same as calling require() from javascript - this is the code that is actually run for that
*/ 
bool require(v8::Local<v8::Context> context, 
                             std::string filename, 
                             v8::Local<v8::Value> & result,
                             const std::vector<std::string> & paths,
                             bool track_modification_times = false, bool use_cache = true);

/**
* prints out a ton of info about a v8::Value
*/
void print_v8_value_details(v8::Local<v8::Value> local_value);

/**
* requires all the files in a directory
*/
void require_directory(v8::Local<v8::Context> context, std::string directory_name);





// void require_directory(std::string directory_name)
// {
//
// // #include <boost/filesystem.hpp>
//     //
//     // boost::filesystem::path p = boost::filesystem::current_path();
//     // boost::filesystem::directory_iterator it{p};
//     // while (it != boost::filesystem::directory_iterator{})
//     //   std::cout << *it++ << '\n';
//     //
//
//     // This probably works on more than just APPLE
// #ifdef __APPLE__
//     DIR * dir = opendir(".");
//     if (dir == NULL)
//             return;
//     struct dirent * dp;
//     while ((dp = readdir(dir)) != NULL) {
//             // if (dp->d_namlen == len && strcmp(dp->d_name, name) == 0) {
//             //         (void)closedir(dir);
//             //         return (FOUND);
//             // }
//     }
//     (void)closedir(dir);
//     return;
//
// #endif // __APPLE__
//
// }


} // end v8toolkit namespace


/** \mainpage v8toolkit API documentation
* v8toolkit is a multi-layer library to simplify working with the V8 Javascript engine.
*
* It contains a set of primitive functions in v8toolkit.h, a library that extends
* the functionality in v8toolkit to user-defined class types in v8_class_wrapper.h, 
* and a set of high level classes for integrating V8 with virtually no knowledge of the
* underlying v8 API in javascript.h.
* 
* Each of these libraries has internal documentation of its types and functions as well
* as an example usage files (any .cpp file with "sample" in its name). 
*/

