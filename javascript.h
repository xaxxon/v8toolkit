#pragma once

#include <thread>
#include <mutex>
#include <future>

#include "v8_class_wrapper.h"

namespace v8toolkit {
	
class IsolateHelper;
class ScriptHelper;


/*
* TODO: Make add_require to context (in addition to isolate) so a particular context
*         ban be disallowed to load external modules but a privileged context on the
*         samea isolate can load them.
*
*
*/

/**
* Wrapper around a v8::Cnotext object with a link back to its associated isolate
* This object can be used wherever a v8::Isolate * or a Local or Global v8::Context
*   is wanted.
* Can only be created via IsolateHelper::create_context()
*/
class ContextHelper : public std::enable_shared_from_this<ContextHelper>
{
    friend class IsolateHelper;
public:
    
    /**
    * Exception class returned to caller when a compilation error is encountered
    * The what() method can be used to get the error string associated with the
    *   error
    */
	class CompilationError : std::exception {
		std::string what_string;
	public:
		CompilationError(std::string what_string) : what_string(what_string) {}
		const char* what() const noexcept override {return what_string.c_str();}
	};
    
    /**
    * Exception class returned to caller when a javascript execution error is 
    *   encountered
    * The what() method can be used to get the error string associated with the
    *   error
    */
	class ExecutionError : std::exception {
        
        /// the actual object thrown from javascript
        v8::Global<v8::Value> exception;
        
		std::string what_string;
        
	public:
		ExecutionError(v8::Global<v8::Value> && exception, std::string what_string) : exception(std::move(exception)), what_string(what_string) {}
		const char* what() const noexcept override {return what_string.c_str();}
	};


private:
	ContextHelper() = delete;
	ContextHelper(const ContextHelper &) = delete;
	ContextHelper(ContextHelper &&) = default;
	ContextHelper & operator=(const ContextHelper &) = delete;
	ContextHelper & operator=(ContextHelper &&) = default;
	
    // isolate_helper MUST be first so it's the last element cleaned up
	/// The IsolateHelper that created this ContextHelper will be kept around as long as a ContextHelper it created is still around
	std::shared_ptr<IsolateHelper> isolate_helper;
	
	/// shortcut to the v8::isolate object instead of always going through the IsolateHelper
	v8::Isolate * isolate;
	
	/// The actual v8::Context object backing this ContextHelper
	v8::Global<v8::Context> context;
    
    /// constructor should only be called by an IsolateHelper
	ContextHelper(std::shared_ptr<IsolateHelper> isolate_helper, v8::Local<v8::Context> context);
    
	
public:

	virtual ~ContextHelper();
	
    /**
    * Implicit cast to v8::Isolate *
    */
	inline operator v8::Isolate*(){return this->isolate;}
    
    /**
    * Implicit cast to v8::Local<v8::Context>
    */
	inline operator v8::Local<v8::Context>(){return this->context.Get(isolate);}
    
    /**
    * Implicit cast to v8::Global<v8::Context>
    */
	inline operator v8::Global<v8::Context>(){return v8::Global<v8::Context>(isolate, this->context.Get(isolate));}
	
	/**
    * Returns a Local copy of the associated v8::Context
    */ 
	v8::Local<v8::Context> get_context();
    
    /**
    * Returns the v8::Isolate * this context is associated with
    */ 
	v8::Isolate * get_isolate();
    
    /**
    * Returns the IsolateHelper wrapping the isolate this context is associated with
    */
	std::shared_ptr<IsolateHelper> get_isolate_helper();
	
	/**
    * Compiles the contents of the passed in string as javascripts
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<ScriptHelper> compile(const std::string);
    
	/**
    * Compiles the contents of the passed in v8::String as javascript
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<ScriptHelper> compile(const v8::Local<v8::String> script);
    
    /**
    * Loads the contents of the given file as javascript
    * Throws v8toolkit::CompilationError on compilation error
    * TODO: what if the file can't be opened?
    */
	std::shared_ptr<ScriptHelper> compile_from_file(const std::string);
	
    /**
    * Runs the previously compiled v8::Script.
    * Throws v8toolkit::ExecutionError on execution error
    */
	v8::Global<v8::Value> run(const v8::Global<v8::Script> & script);
    
    /**
    * Compiles and runs the contents ot the passed in string
    * Throws v8toolkit::CompilationError on compilation error
    * Throws v8toolkit::ExecutionError on execution error
    */
	v8::Global<v8::Value> run(const std::string);
	v8::Global<v8::Value> run(const v8::Local<v8::Value> script);
	
    
	/**
    * Compiles and runs the contents of the passed in string in a std::async and returns
    *   the std::future associated with it.  The future has the result of the javascript
    *   as well as a shared_ptr to the ScriptHelper to make sure the value can still be used
    * 
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in compilation?
    * TODO: what happens if there are errors in execution?
    */
	std::future<std::pair<v8::Global<v8::Value>, std::shared_ptr<ScriptHelper>>> 
        run_async(const std::string,
                  std::launch launch_policy = 
                     std::launch::async | std::launch::deferred);
        
    	
	/**
    * Executes the previously compiled v8::script in a std::thread and returns
    *   the std::thread associated with it.  It must either be joined or detached
    *   before the std::thread object is destroyed
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in execution?
    */
	std::thread run_thread(const v8::Global<v8::Script> & script);

	/**
    * Compiles and runs the contents of the passed in string in a in a std::thread and returns
    *   the std::thread associated with it.  It must either be joined or detached
    *   before the std::thread object is destroyed
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in compilation?
    * TODO: what happens if there are errors in execution?
    */
	std::thread run_thread(const std::string);
    
	/**
    * Executes the previously compiled v8::script in a std::thread and returns
    *   the std::thread associated with it.  It must either be joined or detached
    *   before the std::thread object is destroyed
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in execution?
    */
	std::thread run_thread(const v8::Local<v8::Value> script);
	
	/**
    * Executes the previously compiled v8::script in a detached std::thread.
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in execution?
    */
	void run_detached(const v8::Global<v8::Script> & script);
    
	/**
    * Compiles and runs the contents of the passed in string in a detached std::thread.
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in compilation?
    * TODO: what happens if there are errors in execution?
    */
	void run_detached(const std::string);
    
	/**
    * Executes the previously compiled v8::script in a detached std::thread.
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in execution?
    */
	void run_detached(const v8::Local<v8::Value> script);

	
    /**
    * Calls v8toolkit::scoped_run with the assciated isolate and context data
    */
	template<class T, 
			 class R = decltype(std::declval<T>()()),
			 decltype(std::declval<T>()(), 1) = 1>
	R operator()(T callable)
	{
		v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

    /**
    * Calls v8toolkit::scoped_run with the assciated isolate and context data
    * Passes the v8::Isolate * into the callback
    */
	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
	R operator()(T callable)
	{
		v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

    /**
    * Calls v8toolkit::scoped_run with the assciated isolate and context data
    * Passes the v8::Isolate * and context into the callback
    */
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
    * See: v8toolkit::add_function
	*/
	template<class Function>
	void add_function(std::string name, Function function)
	{
		operator()([&](){
			v8toolkit::add_function(get_context(), get_context()->Global(), name.c_str(), function);
		});
	}
	
	/**
	* Exposes a C++ variable to this context only
    * see: v8toolkit::expose_variable
	*/ 
	template<class Variable>
	void expose_variable(std::string name, Variable & variable)
	{
		v8toolkit::expose_variable(get_context(), get_context()->Global(), name.c_str(), variable);
	}
	
	/**
	* Returns a javascript object representation of the given c++ object
    * see: votoolkit::V8ClassWrapper
	*/
	template<class T>
	auto wrap_object(T* object);
	
};

/**
* Helper class for a v8::Script object.  As long as a ScriptHelper shared_ptr is around,
*   the associated ContextHelper will be maintined (which keeps the IsolateHelper around, too)
*/
class ScriptHelper : public std::enable_shared_from_this<ScriptHelper> 
{
    friend class ContextHelper;
    
private:
    ScriptHelper(std::shared_ptr<ContextHelper> context_helper, v8::Local<v8::Script> script) :
        context_helper(context_helper),
        isolate(*context_helper),
        script(v8::Global<v8::Script>(isolate, script)) {}
    
    // shared_ptr to ContextHelper should be first so it's the last cleaned up
    std::shared_ptr<ContextHelper> context_helper;
    v8::Isolate * isolate;
    v8::Global<v8::Script> script;
    
public:
    
	ScriptHelper() = delete;
	ScriptHelper(const ScriptHelper &) = delete;
	ScriptHelper(ScriptHelper &&) = default;
	ScriptHelper & operator=(const ScriptHelper &) = delete;
	ScriptHelper & operator=(ScriptHelper &&) = default;
    virtual ~ScriptHelper(){
#ifdef V8TOOLKIT_JAVASCRIPT_DEBUG
        printf("Done deleting ScriptHelper\n");
#endif
    }
    
    /**
    * Allows implicit conversion to a v8::Global<v8::Script>
    */
    inline operator v8::Global<v8::Script>&(){return script;}
    
    /**
    * Calls scoped_run with the associated isolate and context
    */
    template<class... Args>
    void operator()(Args&&... args){(*context_helper)(std::forward<Args>(args)...);}
    
    /**
    * Returns the ContextHelper associated with this ScriptHelper
    */
    inline auto get_context_helper(){return context_helper;}
    
    // TODO: Run code should be moved out of contexthelper and into this class
	v8::Global<v8::Value> run(){return context_helper->run(*this);}
    
    /**
    * Run this script in a std:;async and return the associated future.  The future contains a 
    *   shared_ptr to this ScriptHelper so it cannot be destroyed until after the async
    *   has finished and the caller has had a chance to look at the result
    */ 
	auto run_async(std::launch launch_policy = std::launch::async | std::launch::deferred) {
        return std::async(launch_policy, [this](auto script_helper){
            return (*this->context_helper)([this, script_helper](){
                return std::make_pair(this->run(), script_helper);
            });
        }, shared_from_this());
    }

    /**
    * Run this script in a std::thread, returning the std::thread object for joining/detaching.
    * The thread maintains a shared_ptr to this ScriptHelper so it cannot be destroyed
    *   until after the thread completes.
    * Remember, letting the std::thread go out of scope without joinin/detaching is very bad.
    */
	std::thread run_thread(){
        // Holds on to a shared_ptr to the ScriptHelper inside the thread object to make sure
        //   it isn't destroyed until the thread completes
        return std::thread([this](auto script_helper){
            (*this)([this]{
                this->run();
            });
        }, shared_from_this());
    } 
    
    /**
    * Same as run_thread, but the thread is automatically detached.   The ScriptHelper
    *   object is still protected for the lifetime of the 
    */
    void run_detached(){
        run_thread().detach();
    }    
}; 



/**
* Represents a v8::Isolate object.	Any changes made here will be reflected in any
*   contexts created after the change was made.	
* An IsolateHleper will remain as long as the user has a shared_ptr to the IsolateHelper
*   itself or a unique_ptr to any ContextHelper created from the IsolateHelper.
* Can only be created by calling PlatformHelper::create_isolate()
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
    
    /**
    * Adds require() function to javascript as defined in
    * v8toolkit::add_require()
    */
    void add_require();
	
	
    /**
    * Creates a ContextHelper populated by all customizations already
    *   applied to this IsolateHelper, but subsequent customizations to
    *   the IsolateHelper will not be applied to ContextHelpers already
    *   created.
    */
	std::shared_ptr<ContextHelper> create_context();

    /**
    * Returns the isolate associated with this IsolateHelper
    */
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
    * Passes the v8::Isolate * and v8::Local<v8::Context> to the callable function.
    * Throws v8toolkit::InvalidCallException if the isolate is not currently in a context
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
    /** 
    * Initializes the v8 platform with default values and tells v8 to look
    *   for its .bin files in the given directory (often argv[0])
    */ 
    static void init(char * path_to_bin_files);
        
    /**
    * Parses argv for v8-specific options, applies them, and removes them
    *   from argv and adjusts argc accordingly.  Looks in argv[0] for the
    *    v8 .bin files
    */
	static void init(int argc, char ** argv);
    
    /**
    * Shuts down V8.  Any subsequent V8 usage is probably undefined, so 
    *   make sure everything is done before you call this.
    */ 
	static void cleanup();
	
	
	/**
    * Creates a new IsolateHelper wrapping a new v8::Isolate instance.  Each isolate is completely separate from all the others
    *   and each isolate can be used simultaneously across threads (but only 1 thread per isolate at a time)
    * An IsolateHelper will remain as long as the caller has a shared_ptr to the IsolateHelper or any ContextHelpers created from
    *   the IsolateHelper still exist.   Once neither of those things is the case, the IsolateHelper will be automatically destroyed.
    * If any threads are still running when this happens, the results are undefined.
    * TODO: Can active threads maintain links to their ContextHelpers to stop this from happening?
    */
	static std::shared_ptr<IsolateHelper> create_isolate();
};

} // end v8toolkit namespace
