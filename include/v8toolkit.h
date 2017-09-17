
#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <tuple>

#include <string.h>

#include "v8helpers.h"
#include "casts.h"
#include "stdfunctionreplacement.h"

#include "call_callable.h"
#include "exceptions.h"

#ifndef _MSC_VER
#include <dirent.h>
#endif


#define V8_TOOLKIT_DEBUG false


/** TODO LIST
*
* All includes should be included as #include "v8toolkit/FILENAME.h" instead of just #include "FILENAME.h"
* Rename javascript.h file to something much better
* Including "v8toolkit/v8toolkit.h" should include everything, but specific includes should also work for
*   v8helpers.h, <new name for v8toolkit.h>, <new name for javascript.h>
*/



namespace v8toolkit {


struct SetWeakCallbackData {
    SetWeakCallbackData(func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback,
                        v8::Isolate * isolate,
                        const v8::Local<v8::Object> & javascript_object,
                        bool destructive);

    func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback;
    v8::Global<v8::Object> global;
    bool destructive;
};



template<class T>
struct remove_const_from_reference {
    using type = T;
};

template<class T>
struct remove_const_from_reference<T const &>{
    using type = T &&;
};

template<class T>
struct remove_const_from_reference<T const>{
    using type = T;
};

template<class T>
using remove_const_from_reference_t = typename remove_const_from_reference<T>::type;


/**
 * Struct of data passed down through building the parameters to call the function and actually
 * calling the function
 */
template<class R, class... Args>
struct FunctionTemplateData {
    func::function<R(Args...)> callable;
    std::string name;
};




/**
* Creates a function template from a func::function
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate,
                                                       func::function<R(Args...)> f,
                                                       std::string const & name)
{
    auto data = new FunctionTemplateData<R, Args...>();
    data->callable = f;
    data->name = name;

    // wrap the actual call in this lambda
    return v8::FunctionTemplate::New(isolate, [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();

        FunctionTemplateData<R, Args...> & data = *(FunctionTemplateData<R, Args...> *)v8::External::Cast(*(info.Data()))->Value();

        try {
            CallCallable<decltype(data.callable)>()(data.callable, info, std::index_sequence_for<Args...>{});

        } catch (std::exception & e) {

            isolate->ThrowException(v8::String::NewFromUtf8(isolate, e.what()));

            // OLD CODE PUSHED EXCEPTION BACK THROUGH JAVASCRIPT TO C++ BUT EXCEPTION NOT AVAILABLE IN JAVASCRIPT
            //auto anyptr_t = new Any<std::exception_ptr>( std::current_exception());
            // always put in the base ptr so you can cast to it safely and then use dynamic_cast to try to figure
            //   out what it really is
            //isolate->ThrowException(v8::External::New(isolate, static_cast<AnyBase*>(anyptr_t)));
        }
        return; // no return value, PB sets it in the 'info' object

    }, v8::External::New(isolate, (void*)data));
}



/**
 * Takes a class with a const-qualified operator() defined on it and returns a func::function calling it
 */
template<class R, class CLASS, class... Args>
func::function<R(Args...)> make_std_function_from_callable(R(CLASS::*f)(Args...) const, CLASS callable)
{
    return func::function<R(Args...)>(callable);
}

/**
 * Takes a class with an operator() defined on it and returns a func::function calling it
 */
template<class R, class CLASS, class... Args>
func::function<R(Args...)> make_std_function_from_callable(R(CLASS::*f)(Args...), CLASS callable)
{
    return func::function<R(Args...)>(callable);
}


/**
* Creates a v8::FunctionTemplate for an arbitrary callable
*/
template<class T>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate, T callable, std::string name)
{
    return make_function_template(isolate, make_std_function_from_callable(&T::operator(), callable), name);
}


/**
* Creates a function template from a c-style function pointer
*/
template <class R, class... Args>
v8::Local<v8::FunctionTemplate> make_function_template(v8::Isolate * isolate,  R(*f)(Args...), std::string const & name)
{
    return make_function_template(isolate, func::function<R(Args...)>(f), name);
}


/**
* Helper to both create a function template from a func::function and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, func::function<R(Args...)> function) {
    object_template->Set(isolate, name, make_function_template(isolate, function, name));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class T>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T callable) {
        function_type_t<T> f(callable);
    object_template->Set(isolate, name, make_function_template(isolate, f, name));
}

/**
* Helper to both create a function template from an arbitrary function pointer and bind it with the specified name to the specified object template
* Adding functions to an object_template allows creation of multiple contexts with the function already added to each context
*/
template<class R, class... Args>
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, R(*function)(Args...)) {
    object_template->Set(isolate, name, make_function_template(isolate, function, name));
}

/**
* Helper to both create a function template from an arbitrary callable and bind it with the specified name to the specified object template
* Adding functions to an object allows adding a function to any object, including a context's global object.
*/

template<class T>
void add_function(const v8::Local<v8::Context> & context, const v8::Local<v8::Object> & object, const char * name, T callable)
{
    CONTEXT_SCOPED_RUN(context);

    auto isolate = context->GetIsolate();
    auto function_template = make_function_template(isolate, callable, name);
    auto function = function_template->GetFunction(context).ToLocalChecked();
    (void)object->Set(context, v8::String::NewFromUtf8(isolate, name), function);
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
void add_variable(const v8::Local<v8::Context> context,
                  const v8::Local<v8::Object> & object,
                  const char * name,
                  const v8::Local<v8::Value> value);



/**
* add a function that directly handles the v8 callback data
* explicit function typing needed to coerce non-capturing lambdas into c-style function pointers
*/
void add_function(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, void(*function)(const v8::FunctionCallbackInfo<v8::Value>&));


// helper for getting exposed variables
template<class T>
void _variable_getter(v8::Local<v8::String> property,
                      const v8::PropertyCallbackInfo<v8::Value>& info)
{
    auto isolate = info.GetIsolate();
    T * variable = (T*)v8::External::Cast(*(info.Data()))->Value();
//    if (return_most_derived) {
//        //TODO: ME
//        assert(false);
//    } else {
        info.GetReturnValue().Set(CastToJS<T>()(isolate, *variable));
//    }
}


// setter is a no-op if the type is const
template<class T, std::enable_if_t<std::is_const<T>::value, int> = 0>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    // should this throw a V8 exception?
}


// if the type is not const, then set the value as requested
template<class T, std::enable_if_t<!std::is_const<T>::value, int> = 0>
void _variable_setter(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info)
{
    // TODO: This doesnt work well with pointer types - we want to assign to the dereferenced version, most likely.
    *(T*)v8::External::Cast(*(info.Data()))->Value() = CastToNative<T>()(info.GetIsolate(), value);
}
//
//template <class T>
//struct AccessorData {
//    T * variable;
//    bool return_most_derived = false;
//    AccessorData(T * variable, bool return_most_derived = false) :
//        variable(variable),
//        return_most_derived(return_most_derived)
//    {}
//};

/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads and writes to the variable
*/
template<class T>
void expose_variable(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                                 _variable_getter<T>,
                                 _variable_setter<T>,
                                 v8::External::New(isolate, &variable));
}


template<class T, class... Rest>
 void expose_variable(v8::Isolate * isolate,
                      const v8::Local<v8::ObjectTemplate> & object_template,
                      const char * name,
                      std::unique_ptr<T, Rest...> & variable) {
object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                             _variable_getter<std::unique_ptr<T, Rest...>&>,
                             _variable_setter<std::unique_ptr<T, Rest...>&>,
                             v8::External::New(isolate, variable.get()));
}



/**
* Exposes the specified variable to javascript as the specified name in the given object template (usually the global template).
* Allows reads to the variable.  Writes are ignored.
* TODO: consider making writes errors (throw?)
*/
template<class T>
void expose_variable_readonly(v8::Isolate * isolate, const v8::Local<v8::ObjectTemplate> & object_template, const char * name, T & variable) {
    object_template->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                                 _variable_getter<T>,
                                 0,
                                 v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read/write variable
* Often used to add the variable to a context's global object
*/
template<class T>
void expose_variable(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, T & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name),
                        _variable_getter<T>,
                        _variable_setter<T>,
                        v8::External::New(isolate, &variable));
}

/**
* Exposes the C++ variable 'variable' to a specific javascript object as a read-only variable (writes are ignored but are not errors)
* TODO: consider making them errors (throw?)
* Often used to add the variable to a context's global object
*/
template<class T>
void expose_variable_readonly(v8::Local<v8::Context> context, const v8::Local<v8::Object> & object, const char * name, T & variable) {
    auto isolate = context->GetIsolate();
    object->SetAccessor(v8::String::NewFromUtf8(isolate, name), _variable_getter<T>, 0, v8::External::New(isolate, &variable));
}




/**
* Takes a local and creates a weak global reference callback for it
* Useful for clearing out C++-allocated memory on javascript garbage collection of an associated javascript object
 * Remember, this is not guaranteed to ever be called
*/
SetWeakCallbackData * global_set_weak(v8::Isolate * isolate,
                     const v8::Local<v8::Object> & javascript_object,
                     func::function<void(v8::WeakCallbackInfo<SetWeakCallbackData> const &)> callback,
                      bool destructive);



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
void add_print(v8::Isolate * isolate, v8::Local<v8::ObjectTemplate> object_template, func::function<void(const std::string &)> = [](const std::string & s){printf("%s", s.c_str());} );
void add_print(const v8::Local<v8::Context> context, func::function<void(const std::string &)> callback = [](const std::string & s){printf("%s", s.c_str());});

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
* Can be wrapped with a func::function
*/
template<class>
struct Bind{};

/**
 * Non-const object to non-const method
 */
template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...)> {

    Bind(T & object, R(T::*method)(Args...) )
        :
      object(object), method(method)
    {}

    ~Bind(){}

    T & object;
    R(T::*method)(Args...);

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
//        return R();
    }
};

template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) &> {

    Bind(T & object, R(T::*method)(Args...) &) :
            object(object), method(method){}

    ~Bind(){}

    T & object;
    R(T::*method)(Args...) &;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};



/**
 * Non-const object to const method
 */
template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) const> {

    Bind(T const & object, R(T::*method)(Args...) const) :
      object(object), method(method){}

    T const & object;
    R(T::*method)(Args...) const;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};


template<class R, class T, class... Args>
struct Bind<R(T::*)(Args...) const &> {

    Bind(T const & object, R(T::*method)(Args...) const &) :
            object(object), method(method){}

    T const & object;
    R(T::*method)(Args...) const &;

    R operator()(Args && ... params){
        return (object.*method)(std::forward<Args>(params)...);
    }
};





/**
 * unqualified
* Helper function to create a Bind object using type deduction and wrap it in a
* func::function object.
* This specialization is for handling non-const class methods
*/
template <class CLASS, class R, class METHOD_CLASS, class... Args>
    func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))
//    func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))
//    auto bind(CLASS & object, R(METHOD_CLASS::*method)(Args...))

    {
    return Bind<decltype(method)>(object, method);
}


/**
 * l-value qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) &)
{
    return Bind<decltype(method)>(object, method);
}


/**
 * Const qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) const)
{
    return Bind<decltype(method)>(object, method);
}


/**
 * l-value and const qualified
 * @param object
 * @param method
 * @return
 */
template <class CLASS, class R, class METHOD_CLASS, class... Args>
func::function<R(Args...)> bind(CLASS & object, R(METHOD_CLASS::*method)(Args...) const &)
{
    return Bind<decltype(method)>(object, method);
}


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
 * Before deleting an isolate using require, make sure to clean up
 * its require cache or the program will crash while exiting
 * @param isolate which isolate to delete the cache for
 */
void delete_require_cache_for_isolate(v8::Isolate * isolate);


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



struct RequireResult {
    v8::Isolate * isolate;
    v8::Global<v8::Context> context;
    v8::Global<v8::Function> function;
    v8::Global<v8::Value> result;
    time_t time;
    RequireResult(v8::Isolate * isolate,
                  v8::Local<v8::Context> context,
                  v8::Local<v8::Function> function,
                  v8::Local<v8::Value> result,
                  const time_t & time) :
            isolate(isolate),
            context(v8::Global<v8::Context>(isolate, context)),
            function(v8::Global<v8::Function>(isolate, function)),
            result(v8::Global<v8::Value>(isolate, result)),
            time(time)
    {}
    // IF CRASHING IN RequireResult DESTRUCTOR, MAKE SURE TO CALL delete_require_cache_for_isolate BEFORE DESTROYING ISOLATE
};


    /**
* Attempts to load the specified module name from the given paths (in order).
*   Returns the exported object from the module.
* Same as calling require() from javascript - this is the code that is actually run for that
*/
bool require(v8::Local<v8::Context> context,
             std::string filename,
             v8::Local<v8::Value> & result,
             const std::vector<std::string> & paths,
             bool track_modification_times = false,
             bool use_cache = true,
             func::function<void(RequireResult const &)> callback = func::function<void(RequireResult const &)>(),
             func::function<std::string(std::string const &)> resource_name_callback = func::function<std::string(std::string const &)>()
    );


/**
* requires all the files in a directory
*/
void require_directory(v8::Local<v8::Context> context, std::string directory_name);


void dump_prototypes(v8::Isolate * isolate, v8::Local<v8::Object> object);



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

std::vector<std::string> get_interesting_properties(v8::Local<v8::Context> context, v8::Local<v8::Object> object);

v8::Local<v8::Value> run_script(v8::Local<v8::Context> context, v8::Local<v8::Script> script);


void foreach_file(const std::string & directory_name, std::function<void(const std::string &)> const & callback);

void foreach_directory(const std::string & directory_name, std::function<void(const std::string &)> const & callback);

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



#include "casts_impl.h"
