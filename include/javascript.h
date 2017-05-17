#pragma once

#include <thread>
#include <mutex>
#include <future>

#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.



#include "v8_class_wrapper.h"

//#define V8TOOLKIT_JAVASCRIPT_DEBUG

namespace v8toolkit {

class DebugContext;

extern boost::uuids::random_generator uuid_generator;

class Isolate;

class Script;
using ScriptPtr = std::shared_ptr<Script>;


/**
* Wrapper around a v8::Cnotext object with a link back to its associated isolate
* This object can be used wherever a v8::Isolate * or a Local or Global v8::Context
*   is wanted.
* Can only be created via Isolate::create_context()
*/
class Context : public std::enable_shared_from_this<Context>
{
    friend class Isolate;
protected:
	/// constructor should only be called by an Isolate or derived class
	Context(std::shared_ptr<Isolate> isolate_helper, v8::Local<v8::Context> context);


private:
	std::atomic<int> script_id_counter;

	Context() = delete;
	Context(const Context &) = delete;
	Context(Context &&) = default;
	Context & operator=(const Context &) = delete;
	Context & operator=(Context &&) = default;
	
    // isolate_helper MUST be first so it's the last element cleaned up
	/// The Isolate that created this Context will be kept around as long as a Context it created is still around
	std::shared_ptr<Isolate> isolate_helper;
	

	/// The actual v8::Context object backing this Context
	v8::Global<v8::Context> context;



	/// unique identifier for each context
	boost::uuids::uuid const uuid = v8toolkit::uuid_generator();


public:

	/// shortcut to the v8::isolate object instead of always going through the Isolate
	v8::Isolate * const isolate;

	virtual ~Context();

    /**
     * Allows for possible destruction of the Context once all Script objects are released.  This clears out
     * all internal references to scripts to stop any circular references.  The context will have diminished
     * functionality after shutdown is called on it.
     */
    void shutdown();

	/**
	 * Returns all the scripts associated with this context
	 * @return a vector of all the scripts associated with this context
	 */
    std::vector<ScriptPtr> const & get_scripts() const;

    /**
     * Returns a list of functions compiled directly to this context (vs those in a script)
     * @return a list of functions compiled directly to this context (vs those in a script)
     */
    std::vector<v8::Global<v8::Function>> const & get_functions() const;


    /**
     * Registers an externally created script object with this Context and returns a wrapped
     * Script object
     * @param external_script script that was created 'by hand' not with a method on this context
     * @return wrapped v8toolkit::Script object
     */
	void register_external_script(v8::Local<v8::Script> external_script, std::string const & source_code);

	void register_external_function(v8::Global<v8::Function> external_function);


		/**
         * Returns the global context object - useful for GLOBAL_CONTEXT_SCOPED_RUN
         * @return the global context object
         */
	v8::Global<v8::Context> const & get_global_context() const;

    /**
    * Implicit cast to v8::Isolate *
    */
	inline operator v8::Isolate*() const {return this->isolate;}
    
    /**
    * Implicit cast to v8::Local<v8::Context>
    */
	inline operator v8::Local<v8::Context>() const {return this->context.Get(isolate);}
    
    /**
    * Implicit cast to v8::Global<v8::Context>
    */
	inline operator v8::Global<v8::Context>() const {return v8::Global<v8::Context>(isolate, this->context.Get(isolate));}
	
	/**
    * Returns a Local copy of the associated v8::Context
    */ 
	v8::Local<v8::Context> get_context() const;
    
    /**
    * Returns the v8::Isolate * this context is associated with
    */ 
	v8::Isolate * get_isolate() const;
    
    /**
    * Returns the Isolate wrapping the isolate this context is associated with
    */
	std::shared_ptr<Isolate> get_isolate_helper() const;
	
	/**
    * Compiles the contents of the passed in string as javascripts
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<Script> compile(const std::string & source, const std::string & filename = "Unspecified");
    
	/**
    * Compiles the contents of the passed in v8::String as javascript
    * Throws v8toolkit::CompilationError on compilation error
    */
	std::shared_ptr<Script> compile(const v8::Local<v8::String> source, const std::string & filename = "Unspecified");
    
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
	std::future<std::pair<ScriptPtr, v8::Global<v8::Value>>>
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
	auto operator()(Callable && callable) -> std::result_of_t<Callable()>
	{
        GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
		return callable();
	}

    /**
    * Calls v8toolkit::scoped_run with the associated isolate and context data
    * Passes the v8::Isolate * into the callback
    */
	template<class Callable>
	auto operator()(Callable && callable) -> std::result_of_t<Callable(v8::Isolate*)>
	{
		GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
		return callable(isolate);
	}

    /**
    * Calls v8toolkit::scoped_run with the assciated isolate and context data
    * Passes the v8::Isolate * and context into the callback
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of<Callable(v8::Isolate*, v8::Local<v8::Context>)>::type
	{
		GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
		return callable(isolate, context.Get(isolate));
	}
	
	/**
	* Adds a function to this context only
    * See: v8toolkit::add_function
	*/
	template<class Function>
	void add_function(std::string name, Function function)
	{
		GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
		v8toolkit::add_function(get_context(), get_context()->Global(), name.c_str(), function);
	}

	/**
	 * Creates a global javascript variable to the specified context and sets it to the given javascript value.
	 * @param name name of the JavaScript variable
	 * @param variable JavaScript value for the variable to refer to
	 */
    inline void add_variable(std::string name, v8::Local<v8::Value> variable)
    {
		GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
		v8toolkit::add_variable(get_context(), get_context()->Global(), name.c_str(), variable);
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

	boost::uuids::uuid const & get_uuid() const;
	std::string get_uuid_string() const;
	std::string get_url(std::string const & name) const;

    /**
     * Returns the script corresponding to the given id or throws
     * @param script_id script id to find
     * @return script with given script_id
     */
    Script const & get_script_by_id(int64_t script_id);

	v8::Local<v8::Function> get_function_by_id(int64_t script_id);

	/**
	 * Evaluates the specified file and returns the result - can be .js or .json
	 * @param filename file containing javascript or json to evaluate
	 * @return the result of the evaluation or empty on failure
	 */
	v8::Local<v8::Value> require(std::string const & filename, std::vector<std::string> const & paths);
	void require_directory(std::string const & directory_name);

};

using ContextPtr = std::shared_ptr<Context>;


/**
* Helper class for a v8::Script object.  As long as a Script shared_ptr is around,
*   the associated Context will be maintined (which keeps the Isolate around, too)
*/
class Script : public std::enable_shared_from_this<Script>
{
    friend class Context;
    
private:
    Script(ContextPtr context_helper,
           v8::Local<v8::Script> script,
		   std::string const & source_code = "");

    // shared_ptr to Context should be first so it's the last cleaned up
    std::shared_ptr<Context> context_helper;
    v8::Isolate * isolate;
    v8::Global<v8::Script> script;
	std::string script_source_code;

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

    std::string const & get_source_code() const;

	// this should go back to being a ref to an instance variable
    std::string get_source_location() const;
	int64_t get_script_id() const;

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
	* The order of the pair elements matters to make sure the Global is cleaned up before the ScriptPtr
    */ 
	auto run_async(std::launch launch_policy = std::launch::async | std::launch::deferred){

        return std::async(launch_policy, [this](ScriptPtr script)->std::pair<std::shared_ptr<Script>, v8::Global<v8::Value>> {

			return (*this->context_helper)([this, script](){
				return std::make_pair(script, this->run());
            });

		}, shared_from_this());

    }

    /**
    * Run this script in a std::thread, returning the std::thread object for joining/detaching.
    * The thread maintains a shared_ptr to this Script so it cannot be destroyed
    *   until after the thread completes.
    * Remember, letting the std::thread go out of scope without joinin/detaching is very bad.
    */
	std::thread run_thread();


    /**
    * Same as run_thread, but the thread is automatically detached.   The Script
    *   object is still protected for the lifetime of the 
    */
    void run_detached();


	v8::Local<v8::UnboundScript> get_unbound_script() const;

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

    boost::uuids::uuid const uuid = v8toolkit::uuid_generator();


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
    Isolate & add_print(func::function<void(const std::string &)>);
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

    std::shared_ptr<DebugContext> create_debug_context(short port);

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
	    ISOLATE_SCOPED_RUN(isolate);
	    return callable();
	}

	/**
    * wraps "callable" in appropriate thread locks, isolate, and handle scopes
    * Passes the v8::Isolate * to the callable function
    */
	template<class Callable>
	auto operator()(Callable && callable) -> typename std::result_of<Callable(v8::Isolate*)>::type
	{
	    ISOLATE_SCOPED_RUN(isolate);
	    return callable(isolate);
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
	    v8toolkit::V8ClassWrapper<std::add_const_t<T>>::get_instance(this->isolate);
	    return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
		
	}	
    
    /**
    * Returns a value representing the JSON string specified or throws on bad JSON
    */
    v8::Local<v8::Value> json(std::string json) {
        v8::TryCatch tc(this->isolate);
        auto maybe = v8::JSON::Parse(this->isolate, v8::String::NewFromUtf8(this->isolate, json.c_str()));
        if (tc.HasCaught()) {
            throw V8ExecutionException(this->isolate, tc);
        }
        return maybe.ToLocalChecked();
    }

	/**
	 * Returns the debug context for the isolate
	 * @return the debug context for the isolate
	 */
	ContextPtr get_debug_context();
};

using IsolatePtr = std::shared_ptr<Isolate>;

/**
* A singleton responsible for initializing the v8 platform and creating isolate helpers.
*/
class Platform {

	static std::unique_ptr<v8::Platform> platform;
	static v8toolkit::ArrayBufferAllocator allocator;
	static bool initialized;
	static bool expose_gc_value;
	static std::string expose_debug_name;
	static int memory_size_in_mb; // used for Isolate::CreateParams::constraints::set_max_old_space_size()
public:

	// how to increase max memory available to javascript
	//	Isolate::CreateParams::constraints::set_max_old_space_size().


	static void expose_gc();

	static void set_max_memory(int memory_size_in_mb);

    /**
    * Parses argv for v8-specific options, applies them, and removes them
    *   from argv and adjusts argc accordingly.
 	* @param snapshot_directory directory in which to look for v8 snapshot .bin files.  Leave as empty
     * string if linking against v8 compiled with use_snapshots=false
    */
    static void init(int argc, char ** argv, std::string const & snapshot_directory = "");
	
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
    static IsolatePtr create_isolate();
};


template<class T>
v8::Local<v8::Value> Context::wrap_object(T* object)
{
	auto & class_wrapper = V8ClassWrapper<T>::get_instance(this->get_isolate());
	return class_wrapper.template wrap_existing_cpp_object(this->get_context(), object, *class_wrapper.destructor_behavior_leave_alone);
}


} // end v8toolkit namespace
