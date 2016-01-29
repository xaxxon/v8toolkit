#pragma once

#include <thread>
#include <mutex>
#include <future>

#include "v8_class_wrapper.h"

namespace v8toolkit {
	
class IsolateHelper;
	
class ContextHelper {
public:
	class CompilationError : std::exception {
		std::string what_string;
	public:
		CompilationError(std::string what_string) : what_string(what_string) {}
		const char* what() const noexcept {return what_string.c_str();}
	};
	class ExecutionError : std::exception {
		std::string what_string;
	public:
		ExecutionError(std::string what_string) : what_string(what_string) {}
		const char* what() const noexcept {return what_string.c_str();}
	};
	
	
private:
	ContextHelper() = delete;
	ContextHelper(const ContextHelper &) = delete;
	ContextHelper(ContextHelper &&) = default;
	ContextHelper & operator=(const ContextHelper &) = delete;
	ContextHelper & operator=(ContextHelper &&) = default;
	
	/// The IsolateHelper that created this ContextHelper will be kept around as long as a ContextHelper it created is still around
	std::shared_ptr<IsolateHelper> isolate_helper;
	
	/// shortcut to the v8::isolate object instead of always going through the IsolateHelper
	v8::Isolate * isolate;
	
	/// The actual v8::Context object backing this ContextHelper
	v8::Global<v8::Context> context;
	
public:
	ContextHelper(std::shared_ptr<IsolateHelper> isolate_helper, v8::Local<v8::Context> context);
		
	virtual ~ContextHelper();
	
	operator v8::Isolate*(){return this->isolate;}	
	
	
	v8::Local<v8::Context> get_context();
	v8::Isolate * get_isolate();
	std::shared_ptr<IsolateHelper> get_isolate_helper();
	
	
	v8::Global<v8::Script> compile(const std::string);
	v8::Global<v8::Script> compile(const v8::Local<v8::String> script);
	v8::Global<v8::Script> compile_from_file(const std::string);
	
	v8::Global<v8::Value> run(const v8::Global<v8::Script> & script);
	v8::Global<v8::Value> run(const std::string);
	v8::Global<v8::Value> run(const v8::Local<v8::Value> script);
	
	
	// runs the specified code in a thread and returns a future that can be used to retrieve
	//	 the return value of the code
	std::future<v8::Global<v8::Value>> run_async(const v8::Global<v8::Script> & script);
	std::future<v8::Global<v8::Value>> run_async(const std::string);
	std::future<v8::Global<v8::Value>> run_async(const v8::Local<v8::Value> script);
	
	// runs the specified code in a thread, returning the thread object to be joined on
	//	 no return value available
	std::thread run_thread(const v8::Global<v8::Script> & script);
	std::thread run_thread(const std::string);
	std::thread run_thread(const v8::Local<v8::Value> script);
	
	// runs the specified code in a detached thread - no way to know when it's done
	//	 no return value available
	void run_detached(const v8::Global<v8::Script> & script);
	void run_detached(const std::string);
	void run_detached(const v8::Local<v8::Value> script);

	
	template<class T, 
			 class R = decltype(std::declval<T>()()),
			 decltype(std::declval<T>()(), 1) = 1>
	R operator()(T callable)
	{
		v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
	R operator()(T callable)
	{
		v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
	R operator()(T callable)
	{
		v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}
	
	/**
	* Adds a function to this context only
	*/
	template<class Function>
	void add_function(std::string name, Function function)
	{
		operator()([&](){
			v8toolkit::add_function(get_context(), get_context()->Global(), name.c_str(), function);
		});
	}
	
	/**
	* Adds a variable to this context only
	*/ 
	template<class Variable>
	void expose_variable(std::string name, Variable & variable)
	{
		v8toolkit::expose_variable(get_context(), get_context()->Global(), name.c_str(), variable);
	}
	
	/**
	* Returns a javascript object representation of the given c++ object
	*/
	template<class T>
	auto wrap_object(T* object);
	
};

/**
* Represents a v8::Isolate object.	Any changes made here will be reflected in any
* contexts created after the change was made.	
* An IsolateHleper will remain as long as the user has a shared_ptr to the IsolateHelper
* itself or a shared_ptr to any ContextHelper created from the IsolateHelper. 
*/
class IsolateHelper : public std::enable_shared_from_this<IsolateHelper> {
    friend class PlatformHelper; // calls the IsolateHelper's private constructor
private:
	
    // Only called by PlatformHelper
	IsolateHelper(v8::Isolate * isolate);
    
    /// The actual v8::Isolate object represented by this IsolateHelper
	v8::Isolate * isolate;
    
    /**
    * The global object template used when creating a context
    * Contexts will reflect all changes made to this template at the 
    *   time of context creation, but template changes after context
    *   creation will not affect contexts already created, only
    *   subsequently created ones
    */
	v8::Global<v8::ObjectTemplate> global_object_template;
    
    
	
public:

	virtual ~IsolateHelper();
	
    /**
    * Implicit cast to v8::Isolate* so an IsolateHelper can be used
    *   where a v8::Isolate* would otherwise be required
    */ 
	operator v8::Isolate*();
		
    /**
    * Adds print helpers to global object template as defined in 
    *   v8toolkit::add_print()
    */
	void add_print();
	
	
	// can no longer use the global_object_template after the context has been created, which means no 
	//	 more 
	std::unique_ptr<ContextHelper> create_context();

	v8::Isolate * get_isolate();

	/**
	* Returns the v8::ObjectTemplate used to create contexts from this IsolateHelper
	*/
	v8::Local<v8::ObjectTemplate> get_object_template();
	
	
	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    */
	template<class T, 
			 class R = decltype(std::declval<T>()()),
			 decltype(std::declval<T>()(), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}

	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    * Passes the v8::Isolate * to the callable function
    */
	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}

	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    * Passes the v8::Isolate * and v8::Local<v8::Context> to the callable function if 
    *   the isolate is currently in a context.   If it's not, this call will throw an exception
    * TODO: Make it throw an exception if no context is available
    */
	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}
	
    /**
    * Adds the specified callable as <name> in javascript
    * This can be a standard C++ function, a lambda, or anything else that supports
    *   being called with operator()
    */
	template<class Callable>
	void add_function(std::string name, Callable callable) 
	{		
		(*this)([&](){
			v8toolkit::add_function(isolate, this->get_object_template(), name.c_str(), callable);
		});
	}
	
    /**
    * Exposes an existing C++ variable as <name> to javascript
    */
	template<class Variable>
	void expose_variable(std::string name, Variable & variable)
	{
		v8toolkit::expose_variable(isolate, this->get_object_template(), name.c_str(), variable);
	}
	
	/**
	* Returns a V8ClassWrapper object for wrapping C++ classes in this isolate.	 Classes must be wrapped
	* before contexts using them are created
	*/
	template<class T>
	auto & wrap_class() {
		return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	}	
};

/**
* A singleton responsible for initializing the v8 platform and creating isolate helpers.
*/
class PlatformHelper {
	
	static std::unique_ptr<v8::Platform> platform;
	static v8toolkit::ArrayBufferAllocator allocator;
	static bool initialized;
	
	
public:
	static void init(int argc, char ** argv);
	static void cleanup();
	
	
	// as long as the user has a shared_ptr to the IsolateHelper or a context created by the IsolateHelper,
	//	 the IsolateHelper will be kept around
	static std::shared_ptr<IsolateHelper> create_isolate();
};

} // end v8toolkit namespace
