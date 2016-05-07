#pragma once

#include <thread>
#include <mutex>
#include <future>
#include <functional>

#include "v8_class_wrapper.h"

namespace v8toolkit {
	
class Isolate;
class Script;




/**
* Wrapper around a v8::Cnotext object with a link back to its associated isolate
* This object can be used wherever a v8::Isolate * or a Local or Global v8::Context
*   is wanted.
* Can only be created via Isolate::create_context()
*/
class Context : public std::enable_shared_from_this<Context>
{
    friend class Isolate;
public:

private:
	Context() = delete;
	Context(const Context &) = delete;
	Context(Context &&) = default;
	Context & operator=(const Context &) = delete;
	Context & operator=(Context &&) = default;
	
    // isolate_helper MUST be first so it's the last element cleaned up
	/// The Isolate that created this Context will be kept around as long as a Context it created is still around
	std::shared_ptr<Isolate> isolate_helper;
	
	/// shortcut to the v8::isolate object instead of always going through the Isolate
	v8::Isolate * isolate;
	
	/// The actual v8::Context object backing this Context
	v8::Global<v8::Context> context;
    
    /// constructor should only be called by an Isolate
	Context(std::shared_ptr<Isolate> isolate_helper, v8::Local<v8::Context> context);
    
	
public:

	virtual ~Context();
	
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
    * Returns the Isolate wrapping the isolate this context is associated with
    */
	std::shared_ptr<Isolate> get_isolate_helper();
	
	/**
    * Compiles the contents of the passed in string as javascripts
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<Script> compile(const std::string & source);
    
	/**
    * Compiles the contents of the passed in v8::String as javascript
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<Script> compile(const v8::Local<v8::String> script);
    
    /**
    * Loads the contents of the given file as javascript
    * Throws v8toolkit::CompilationError on compilation error
    * TODO: what if the file can't be opened?
    */
	std::shared_ptr<Script> compile_from_file(const std::string & filename);
	
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
	v8::Global<v8::Value> run(const std::string & source);
	v8::Global<v8::Value> run(const v8::Local<v8::Value> script);
	
	v8::Global<v8::Value> run_from_file(const std::string & filename);
    
	/**
    * Compiles and runs the contents of the passed in string in a std::async and returns
    *   the std::future associated with it.  The future has the result of the javascript
    *   as well as a shared_ptr to the Script to make sure the value can still be used
    * 
    * While any number of threaded calls can be made, only one context per
    *   isolate can be actively running at a time.   Additional calls will be
    *   queued but will block until they can acquire the v8::Locker object for
    *   their isolate
    * TODO: what happens if there are errors in compilation?
    * TODO: what happens if there are errors in execution?
    */
	std::future<std::pair<v8::Global<v8::Value>, std::shared_ptr<Script>>> 
        run_async(const std::string & source,
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
	std::thread run_thread(const std::string & source);
    
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
	void run_detached(const std::string & source);
    
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
    * Calls v8toolkit::scoped_run with the associated isolate and context data
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of<Callable()>::type
	{
        v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

    /**
    * Calls v8toolkit::scoped_run with the associated isolate and context data
    * Passes the v8::Isolate * into the callback
    */
	template<class Callable>
	auto operator()(Callable && callable) -> std::result_of_t<Callable(v8::Isolate*)>
	{
        v8::Locker l(isolate);
		v8::HandleScope hs(isolate);
		return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
	}

    /**
    * Calls v8toolkit::scoped_run with the assciated isolate and context data
    * Passes the v8::Isolate * and context into the callback
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of<Callable(v8::Isolate*, v8::Local<v8::Context>)>::type
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
    
    template<class T>
    void add_variable(std::string name, v8::Local<T> variable)
    {
		operator()([&](){
			v8toolkit::add_variable(get_context(), get_context()->Global(), name.c_str(), variable);
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
    
    template<class Variable>
    void expose_variable_readonly(std::string name, Variable & variable)
    {
		v8toolkit::expose_variable_readonly(get_context(), get_context()->Global(), name.c_str(), variable);
        
    }
    
    v8::Local<v8::Value> json(const std::string & json);   
    
    
	
	/**
	* Returns a javascript object representation of the given c++ object
    * see: votoolkit::V8ClassWrapper
	*/
	template<class T>
	v8::Local<v8::Value> wrap_object(T* object);
	
};

using ContextPtr = std::shared_ptr<Context>;


/**
* Helper class for a v8::Script object.  As long as a Script shared_ptr is around,
*   the associated Context will be maintined (which keeps the Isolate around, too)
*/
class Script;
using ScriptPtr = std::shared_ptr<Script>;
class Script : public std::enable_shared_from_this<Script> 
{
    friend class Context;
    
private:
    Script(std::shared_ptr<Context> context_helper, v8::Local<v8::Script> script) :
        context_helper(context_helper),
        isolate(*context_helper),
        script(v8::Global<v8::Script>(isolate, script)) {}
    
    // shared_ptr to Context should be first so it's the last cleaned up
    std::shared_ptr<Context> context_helper;
    v8::Isolate * isolate;
    v8::Global<v8::Script> script;
    
public:
    
	Script() = delete;
	Script(const Script &) = delete;
	Script(Script &&) = default;
	Script & operator=(const Script &) = delete;
	Script & operator=(Script &&) = default;
    virtual ~Script(){
#ifdef V8TOOLKIT_JAVASCRIPT_DEBUG
        printf("Done deleting Script\n");
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
    * Returns the Context associated with this Script
    */
    inline auto get_context_helper(){return context_helper;}
    
    // TODO: Run code should be moved out of contexthelper and into this class
	v8::Global<v8::Value> run(){return context_helper->run(*this);}
    
    /**
    * Run this script in a std::async and return the associated future.  The future value is a 
    *   std::pair<javascript_result, shared_ptr<Script>>.  It contains a 
    *   shared_ptr to the Script so the Script (and it's associated dependencies) 
    *   cannot be destroyed until after the async has finished and the caller has had a chance 
    *   to use the results contained in the future
    */ 
	auto run_async(std::launch launch_policy = std::launch::async | std::launch::deferred){

        return std::async(launch_policy, [this](ScriptPtr script)->std::pair<v8::Global<v8::Value>, std::shared_ptr<Script>> {
        
			return (*this->context_helper)([this, script](){
				return std::make_pair(this->run(), script);
            });
        
		}, shared_from_this());

    }

    /**
    * Run this script in a std::thread, returning the std::thread object for joining/detaching.
    * The thread maintains a shared_ptr to this Script so it cannot be destroyed
    *   until after the thread completes.
    * Remember, letting the std::thread go out of scope without joinin/detaching is very bad.
    */
	std::thread run_thread()
	{
        // Holds on to a shared_ptr to the Script inside the thread object to make sure
        //   it isn't destroyed until the thread completes
		// return type must be specified for Visual Studio 2015.2
		// https://connect.microsoft.com/VisualStudio/feedback/details/1557383/nested-generic-lambdas-fails-to-compile-c
        return std::thread([this](auto script_helper)->void{ 
            (*this)([this]{
                this->run();
            });
        }, shared_from_this());
    }
    
    /**
    * Same as run_thread, but the thread is automatically detached.   The Script
    *   object is still protected for the lifetime of the 
    */
    void run_detached(){
        run_thread().detach();
    }    
}; 



/**
* Represents a v8::Isolate object.	Any changes made here will be reflected in any
*   contexts created after the change was made.	
* An IsolateHleper will remain as long as the user has a shared_ptr to the Isolate
*   itself or a unique_ptr to any Context created from the Isolate.
* Can only be created by calling Platform::create_isolate()
*/
class Isolate : public std::enable_shared_from_this<Isolate> {
    friend class Platform; // calls the Isolate's private constructor
private:
	
    // Only called by Platform
	Isolate(v8::Isolate * isolate);
    
    /// The actual v8::Isolate object represented by this Isolate
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

	virtual ~Isolate();
	
    /**
    * Implicit cast to v8::Isolate* so an Isolate can be used
    *   where a v8::Isolate* would otherwise be required
    */ 
	operator v8::Isolate*();
    
    /**
    * Implicit cast to Local<ObjectTemplate> for the global object
    *   template used to create new contexts
    */
    operator v8::Local<v8::ObjectTemplate>();
		
    /**
    * Adds print helpers to global object template as defined in 
    *   v8toolkit::add_print()
    */
    Isolate & add_print(std::function<void(const std::string &)>);
    Isolate & add_print();
    
    void add_assert();
    
    /**
    * Adds require() function to javascript as defined in
    * v8toolkit::add_require()
    */
    void add_require(std::vector<std::string> paths=std::vector<std::string>{"./"});
	
    void add_module_list(){(*this)([this]{v8toolkit::add_module_list(isolate, global_object_template.Get(isolate));});}
	
    /**
    * Creates a Context populated by all customizations already
    *   applied to this Isolate, but subsequent customizations to
    *   the Isolate will not be applied to Contexts already
    *   created.
    */
	std::shared_ptr<Context> create_context();

    /**
    * Returns the isolate associated with this Isolate
    */
	v8::Isolate * get_isolate();

	/**
	* Returns the v8::ObjectTemplate used to create contexts from this Isolate
	*/
	v8::Local<v8::ObjectTemplate> get_object_template();
	
	
	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    */
	template<class Callable>
	auto operator()(Callable && callable) -> std::result_of_t<Callable()>
	{
		return v8toolkit::scoped_run(isolate, std::forward<Callable>(callable));
	}

	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    * Passes the v8::Isolate * to the callable function
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of<Callable(v8::Isolate*)>::type
	{
		return v8toolkit::scoped_run(isolate, std::forward<Callable>(callable));
	}

	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    * Passes the v8::Isolate * and v8::Local<v8::Context> to the callable function.
    * Throws v8toolkit::InvalidCallException if the isolate is not currently in a context
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of_t<Callable(v8::Isolate*, v8::Local<v8::Context>)>
	{
		return v8toolkit::scoped_run(isolate, std::forward<Callable>(callable));
	}
	
    /**
    * Adds the specified callable as <name> in javascript
    * This can be a standard C++ function, a lambda, or anything else that supports
    *   being called with operator()
    */
	template<class Callable>
	void add_function(std::string name, Callable && callable)
	{		
		(*this)([&](){
			v8toolkit::add_function(isolate,
									this->get_object_template(),
									name.c_str(),
									std::forward<Callable>(callable));
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
    
    template<class Variable>
    void expose_variable_readonly(std::string name, Variable & variable)
    {
		v8toolkit::expose_variable_readonly(isolate, this->get_object_template(), name.c_str(), variable);
    }

	/// Not sure what this is used for
    void add_variable(const std::string & name, v8::Local<v8::ObjectTemplate> template_to_attach)
    {
        v8toolkit::add_variable(this->isolate, this->get_object_template(), name.c_str(), template_to_attach);
    }
	
	/**
	* Returns a V8ClassWrapper object for wrapping C++ classes in this isolate.	 Classes must be wrapped
	* before contexts using them are created
	*/
	template<class T>
	auto & wrap_class() {
		return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	}	
    
    /**
    * Returns a value representing the JSON string specified or throws on bad JSON
    */
    v8::Local<v8::Value> json(std::string json) {
        v8::TryCatch tc(this->isolate);
        auto maybe = v8::JSON::Parse(this->isolate, v8::String::NewFromUtf8(this->isolate, json.c_str()));
        if (tc.HasCaught()) {
            throw V8ExecutionException(this->isolate, tc.Exception());
        }
        return maybe.ToLocalChecked();
    }
};

using IsolatePtr = std::shared_ptr<Isolate>;

/**
* A singleton responsible for initializing the v8 platform and creating isolate helpers.
*/
class Platform {
	
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
    * Creates a new Isolate wrapping a new v8::Isolate instance.
    * An Isolate will remain as long as the caller has a shared_ptr to the Isolate or any Contexts created from
    *   the Isolate still exist.
    */
	static std::shared_ptr<Isolate> create_isolate();
};


template<class T>
v8::Local<v8::Value> Context::wrap_object(T* object)
{
    return get_isolate_helper()->wrap_class<T>().wrap_existing_cpp_object(object);
}


} // end v8toolkit namespace
