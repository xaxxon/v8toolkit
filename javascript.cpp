#include <fstream>
#include <memory>
	

#include "javascript.h"

namespace v8toolkit {



ContextHelper::ContextHelper(std::shared_ptr<IsolateHelper> isolate_helper, v8::Local<v8::Context> context) : 
	isolate_helper(isolate_helper), isolate(isolate_helper->get_isolate()), context(v8toolkit::make_global(isolate, context)) 
{}



v8::Local<v8::Context> ContextHelper::get_context(){
	return context.Get(isolate);
}

v8::Isolate * ContextHelper::get_isolate() 
{
	return this->isolate;
}
std::shared_ptr<IsolateHelper> ContextHelper::get_isolate_helper()
{
	return this->isolate_helper;
}



ContextHelper::~ContextHelper() {
	// fprintf(stderr, "destroying context helper\n");
	this->context.Reset();
}


v8::Global<v8::Script> ContextHelper::compile_from_file(const char * filename)
{
	return this->compile(get_file_contents(filename).c_str());
}

v8::Global<v8::Script> ContextHelper::compile(const char * javascript_source)
{
	return v8toolkit::scoped_run(isolate, context.Get(isolate), [&](){
	
		// This catches any errors thrown during script compilation
	    v8::TryCatch try_catch(isolate);
	
		v8::Local<v8::String> source =
		    v8::String::NewFromUtf8(this->isolate, javascript_source);

		// Compile the source code.
		v8::MaybeLocal<v8::Script> compiled_script = v8::Script::Compile(context.Get(isolate), source);
	    if (compiled_script.IsEmpty()) {
		    v8::String::Utf8Value exception(try_catch.Exception());
			printf("Compile failed '%s', throwing exception\n", *exception);
			throw CompilationError(*exception);
	    }

		return v8::Global<v8::Script>(isolate, compiled_script.ToLocalChecked());
	});

}

v8::Global<v8::Value> ContextHelper::run(const v8::Global<v8::Script> & script)
{
	return v8toolkit::scoped_run(isolate, context.Get(isolate), [&](){
	
		// This catches any errors thrown during script compilation
	    v8::TryCatch try_catch(isolate);
	
		// auto local_script = this->get_local(script);
		auto local_script = v8::Local<v8::Script>::New(isolate, script);
	    auto maybe_result = local_script->Run(context.Get(isolate));
		if(maybe_result.IsEmpty()) {
			// printf("Execution failed, throwing exception\n");
		    v8::String::Utf8Value exception(try_catch.Exception());
			throw ExecutionError(*exception);
		}

		v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
		// printf("Run result is object? %s\n", result->IsObject() ? "Yes" : "No");
		// printf("Run result is string? %s\n", result->IsString() ? "Yes" : "No");
	    // Convert the result to an UTF8 string and print it.
	    v8::String::Utf8Value utf8(result);
	    // printf("run script result: %s\n", *utf8);
	
		return v8::Global<v8::Value>(isolate, result);
	});
}


v8::Global<v8::Value> ContextHelper::run(const char * code)
{
	auto compiled_code = compile(code);
	return run(compiled_code);
}

v8::Global<v8::Value> ContextHelper::run(const v8::Local<v8::Value> value)
{
	return run(*v8::String::Utf8Value(value));
}

std::future<v8::Global<v8::Value>> ContextHelper::run_async(const v8::Global<v8::Script> & script)
{
	return isolate_helper->run_async(*this, [this, &script](){return this->run(script);});
}

std::future<v8::Global<v8::Value>> ContextHelper::run_async(const char * code)
{
	return isolate_helper->run_async(*this, [this, code](){return this->run(code);});
}

std::future<v8::Global<v8::Value>> ContextHelper::run_async(const v8::Local<v8::Value> script)
{
	return isolate_helper->run_async(*this, [this, &script](){return this->run(script);});
}




IsolateHelper::IsolateHelper(v8::Isolate * isolate) : isolate(isolate)
{	
	v8toolkit::scoped_run(isolate, [this](auto isolate){
		this->global_object_template.Reset(isolate, v8::ObjectTemplate::New(this->get_isolate()));
	});
}

std::unique_ptr<ContextHelper> IsolateHelper::create_context()
{
	return operator()([this](){
		auto ot = this->get_object_template();
		auto context = v8::Context::New(this->isolate, NULL, ot);
	
		return std::make_unique<ContextHelper>(shared_from_this(), context);
	});
}

v8::Local<v8::ObjectTemplate> IsolateHelper::get_object_template()
{
	return global_object_template.Get(isolate);
}

IsolateHelper::~IsolateHelper()
{
	// fprintf(stderr, "Deleting isolate helper %p for isolate %p\n", this, this->isolate);
	this->global_object_template.Reset();
	this->isolate->Dispose();
}


void PlatformHelper::init(int argc, char ** argv) 
{
	assert(!initialized);
	process_v8_flags(argc, argv);
	
	// Initialize V8.
	v8::V8::InitializeICU();
	
	// startup data is in the current directory
	v8::V8::InitializeExternalStartupData(argv[0]);
	
	PlatformHelper::platform = std::unique_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform());
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();
	
	initialized = true;
}

void PlatformHelper::cleanup()
{

	// Dispose the isolate and tear down V8.
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	
	platform.release();
};

std::shared_ptr<IsolateHelper> PlatformHelper::create_isolate()
{
	assert(initialized);
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = (v8::ArrayBuffer::Allocator *) &PlatformHelper::allocator;

	return std::make_shared<IsolateHelper>(v8::Isolate::New(create_params));
}


bool PlatformHelper::initialized = false;
std::unique_ptr<v8::Platform> PlatformHelper::platform;
v8toolkit::ArrayBufferAllocator PlatformHelper::allocator;


} // end v8toolkit namespace


//
// IsolateHelper::init(argc, argv);
// auto javascript_engine = std::make_unique<JavascriptEngine>();
//
// v8::HandleScope hs(javascript_engine->get_isolate());
// v8::Isolate::Scope is(javascript_engine->get_isolate());
// auto context = 	javascript_engine->get_local(javascript_engine->get_context());
// auto isolate = javascript_engine->get_isolate();
//
// javascript_engine->compile("var foo=4;\r\n--2-32");
//
//
// javascript_engine->add_require(); // adds the require function to the global javascript object
// auto coffeescript_compiler = javascript_engine->compile_from_file("/Users/xaxxon/Downloads/jashkenas-coffeescript-f26d33d/extras/coffee-script.js");
// javascript_engine->run(coffeescript_compiler);
// auto go = context->Global();
// auto some_coffeescript = get_file_contents("some.coffee");
//
// // after the context has been created, changing the template doesn't do anything.  You set the context's global object instead
// go->Set(context, v8::String::NewFromUtf8(isolate, "coffeescript_source"), v8::String::NewFromUtf8(isolate, some_coffeescript.c_str()));
//
// auto compiled_coffeescript = javascript_engine->run("CoffeeScript.compile(coffeescript_source)");
// printf("compiled coffeescript is string? %s\n", compiled_coffeescript->IsString() ? "Yes" : "No");
//
// // this line apparently needs an isolate scope
// std::cout << *v8::String::Utf8Value(compiled_coffeescript) << std::endl;
// javascript_engine->run(compiled_coffeescript);
//

