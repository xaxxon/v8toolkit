#include <fstream>
#include <memory>

#include "javascript.h"

namespace v8toolkit {



v8toolkit::ArrayBufferAllocator JavascriptEngine::allocator;

std::unique_ptr<v8::Platform> JavascriptEngine::platform;
static bool initialized = false;

void JavascriptEngine::init(int argc, char ** argv) 
{
	
	process_v8_flags(argc, argv);
	
	// Initialize V8.
	v8::V8::InitializeICU();
	
	// startup data is in the current directory
	v8::V8::InitializeExternalStartupData(argv[0]);
	
	JavascriptEngine::platform = std::unique_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform());
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();
	
	initialized = true;
}


JavascriptEngine::JavascriptEngine()
{	
	assert(initialized);	
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = (v8::ArrayBuffer::Allocator *) &JavascriptEngine::allocator;

	this->isolate = v8::Isolate::New(create_params);
	
	v8toolkit::scoped_run(isolate, [this](auto isolate){
		this->global_object_template.Reset(isolate, v8::ObjectTemplate::New(this->get_isolate()));
	});
}

v8::Local<v8::Context> JavascriptEngine::create_context()
{
	auto ot = this->get_object_template();
	auto local_context = v8::Context::New(this->isolate, NULL, ot);
	
	ot->Set(this->get_isolate(), "coffeescript_source", v8::String::NewFromUtf8(this->get_isolate(), "a:4"));
	
	
    this->context = v8::Global<v8::Context>(this->get_isolate(), local_context);
	context_created = true;	
	return local_context;
}

v8::Local<v8::ObjectTemplate> JavascriptEngine::get_object_template()
{
	// if the context has been created, the object template isn't of any more use
	assert(!context_created);
	return global_object_template.Get(isolate);
	
}

JavascriptEngine::~JavascriptEngine()
{
	printf("Isolate %p\n", this->isolate);
	this->global_object_template.Reset();
	this->context.Reset();
	this->isolate->Dispose();
}

v8::Global<v8::Script> JavascriptEngine::compile_from_file(const char * filename)
{
	return this->compile(get_file_contents(filename).c_str());
}

v8::Global<v8::Script> JavascriptEngine::compile(const char * javascript_source)
{
	assert(context_created);
	v8::HandleScope hs(this->get_isolate());
	v8::Context::Scope context_scope(context.Get(isolate));
	
	// This catches any errors thrown during script compilation
    v8::TryCatch try_catch(this->get_isolate());
	
	assert(context_created);
	v8::Local<v8::String> source =
	    v8::String::NewFromUtf8(this->isolate, javascript_source,
	                        v8::NewStringType::kNormal).ToLocalChecked();

	// Compile the source code.
	v8::MaybeLocal<v8::Script> compiled_script = v8::Script::Compile(context.Get(isolate), source);
    if (compiled_script.IsEmpty()) {
	    v8::String::Utf8Value exception(try_catch.Exception());
		throw CompilationError(*exception);
    }
	
	return v8::Global<v8::Script>(this->get_isolate(), compiled_script.ToLocalChecked());
	
}

v8::Local<v8::Value> JavascriptEngine::run(const v8::Global<v8::Script> & script)
{
	auto isolate = this->get_isolate();
	return v8toolkit::scoped_run(isolate, context.Get(isolate), [this, &script](v8::Isolate * isolate){
	
		// auto local_script = this->get_local(script);
		auto local_script = v8::Local<v8::Script>::New(this->get_isolate(), script);
	    auto maybe_result = local_script->Run(context.Get(isolate));
		assert(maybe_result.IsEmpty() == false);

		v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
		printf("Run result is object? %s\n", result->IsObject() ? "Yes" : "No");
		printf("Run result is string? %s\n", result->IsString() ? "Yes" : "No");
	    // Convert the result to an UTF8 string and print it.
	    v8::String::Utf8Value utf8(result);
	    printf("run script result: %s\n", *utf8);
	
		return result;
	});
}


v8::Local<v8::Value> JavascriptEngine::run(const char * code)
{
	v8::EscapableHandleScope ehs(this->get_isolate());
	return ehs.Escape(run(compile(code)));
}

v8::Local<v8::Value> JavascriptEngine::run(const v8::Local<v8::Value> value)
{
	v8::EscapableHandleScope ehs(this->get_isolate());
	return ehs.Escape(run(*v8::String::Utf8Value(value)));
}

void JavascriptEngine::cleanup()
{

	// Dispose the isolate and tear down V8.
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	
	platform.release();
}




} // end v8toolkit namespace


//
// JavascriptEngine::init(argc, argv);
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


