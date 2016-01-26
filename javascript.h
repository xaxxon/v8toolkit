#pragma once

#include "v8_class_wrapper.hpp"

namespace v8toolkit {

class JavascriptEngine {
public:

	class CompilationError : std::exception {
		std::string what_string;
	public:
		CompilationError(std::string what_string) : what_string(what_string) {}
		const char* what() const noexcept {return what_string.c_str();}
	};
	
private:
	
	
	static std::unique_ptr<v8::Platform> platform;
	v8::Isolate * isolate;
	v8::Global<v8::Context> context;
	v8::Global<v8::ObjectTemplate> global_object_template;
	static v8toolkit::ArrayBufferAllocator allocator;
	bool context_created = false;
	
public:
	
	static void init(int argc, char ** argv);
	static void cleanup();
	
	JavascriptEngine();
	virtual ~JavascriptEngine();
	
	// can no longer use the global_object_template after the context has been created, which means no 
	//   more 
	v8::Local<v8::Context> create_context();
		
	v8::Isolate * get_isolate() {return this->isolate;}
	
	v8::Global<v8::Script> compile(const char *);
	v8::Global<v8::Script> compile(const v8::Local<v8::String> script);
	v8::Global<v8::Script> compile_from_file(const char *);
	
	v8::Local<v8::Value> run(const v8::Global<v8::Script> & script);
	v8::Local<v8::Value> run(const char *);
	v8::Local<v8::Value> run(const v8::Local<v8::Value> script);
	
	v8::Local<v8::ObjectTemplate> get_object_template();
	
	template<class T>
	auto & wrap_class() {
		return v8toolkit::V8ClassWrapper<T>::get_instance(this->isolate);
	}
	
	auto get_context(){
		assert(context_created);
		return context.Get(isolate);
	}
	auto get_global_object_template(){assert(!context_created); return this->global_object_template.Get(isolate);}
	
	
	
	template<class T, 
			 class R = decltype(std::declval<T>()()),
			 decltype(std::declval<T>()(), 1) = 1>
	R operator()(T callable)
	{
		if (context_created) {
			return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
		} else {
			return v8toolkit::scoped_run(isolate, callable);
		}
	}


	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr))),
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr)), 1) = 1>
	R operator()(T callable)
	{
		if (context_created) {
			return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
		} else {
			return v8toolkit::scoped_run(isolate, callable);
		}
	}

	template<class T, 
			 class R = decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>())), 
			 decltype(std::declval<T>()(static_cast<v8::Isolate*>(nullptr), v8::Local<v8::Context>()), 1) = 1>
	R operator()(T callable)
	{
		if (context_created) {
			return v8toolkit::scoped_run(isolate, context.Get(isolate), callable);
		} else {
			return v8toolkit::scoped_run(isolate, callable);
		}
	}
	
	
	void add_variable(std::string name, v8::Local<v8::Value> value) {
		if (context_created) {
			v8toolkit::add_variable(this->isolate, this->get_object_template(), name.c_str(), value);
		} else {
			auto local_context = this->context.Get(isolate);
			v8toolkit::add_variable(local_context, local_context->Global(), name.c_str(), value);
		}
	}
};


} // end v8toolkit namespace