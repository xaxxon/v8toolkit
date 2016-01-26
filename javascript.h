#pragma once

#include "v8_class_wrapper.hpp"

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
	
private:
	ContextHelper() = delete;
	ContextHelper(const ContextHelper &) = delete;
	ContextHelper(const ContextHelper &&) = delete;
	ContextHelper & operator=(const ContextHelper &) = delete;
	ContextHelper & operator=(const ContextHelper &&) = delete;
	
	/// The IsolateHelper that created this ContextHelper will be kept around as long as a ContextHelper it created is still around
	std::shared_ptr<IsolateHelper> isolate_helper;
	
	/// shortcut to the v8::isolate object instead of always going through the IsolateHelper
	v8::Isolate * isolate;
	
	/// The actual v8::Context object backing this ContextHelper
	v8::Global<v8::Context> context;
	
	
public:
	ContextHelper(std::shared_ptr<IsolateHelper> isolate_helper, v8::Local<v8::Context> context);
		
	virtual ~ContextHelper() {
		this->context.Reset();
	}
	
	
	auto get_context(){
		return context.Get(isolate);
	}
	
	
	
	v8::Global<v8::Script> compile(const char *);
	v8::Global<v8::Script> compile(const v8::Local<v8::String> script);
	v8::Global<v8::Script> compile_from_file(const char *);
	
	v8::Local<v8::Value> run(const v8::Global<v8::Script> & script);
	v8::Local<v8::Value> run(const char *);
	v8::Local<v8::Value> run(const v8::Local<v8::Value> script);
	
	
};

class IsolateHelper : public std::enable_shared_from_this<IsolateHelper> {
private:
	
	v8::Isolate * isolate;
	v8::Global<v8::ObjectTemplate> global_object_template;
	
public:
	
	
	IsolateHelper(v8::Isolate * isolate);
	virtual ~IsolateHelper();
	
	
	// can no longer use the global_object_template after the context has been created, which means no 
	//   more 
	std::unique_ptr<ContextHelper> create_context();
		
	v8::Isolate * get_isolate() {return this->isolate;}
	
	
	v8::Local<v8::ObjectTemplate> get_object_template();
	
	template<class T>
	auto & wrap_class() {
		return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	}
	auto get_global_object_template(){return this->global_object_template.Get(isolate);}
	
	
	
	template<class T, 
			 class R = decltype(std::declval<T>()()),
			 decltype(std::declval<T>()(), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}


	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}

	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
	R operator()(T callable)
	{
		return v8toolkit::scoped_run(isolate, callable);
	}
	
	template<class Function>
	void add_function(std::string name, Function function) 
	{
		v8toolkit::add_function(isolate, this->get_object_template(), name.c_str(), function);
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
	//   the IsolateHelper will be kept around
	static std::shared_ptr<IsolateHelper> create_isolate();
};

} // end v8toolkit namespace