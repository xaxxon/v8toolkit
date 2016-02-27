#include <fstream>
#include <memory>
    
#include "javascript.h"


namespace v8toolkit {

ContextHelper::ContextHelper(std::shared_ptr<IsolateHelper> isolate_helper, v8::Local<v8::Context> context) : 
    isolate_helper(isolate_helper), isolate(isolate_helper->get_isolate()), context(v8::Global<v8::Context>(isolate, context)) 
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

v8::Local<v8::Value> ContextHelper::json(std::string json) {
    return this->isolate_helper->json(json);
}


ContextHelper::~ContextHelper() {    
#ifdef V8TOOLKIT_JAVASCRIPT_DEBUG
    printf("Deleting ContextHelper\n");  
#endif
}


std::shared_ptr<ScriptHelper> ContextHelper::compile_from_file(const std::string filename)
{
    std::string contents;
    time_t modification_time = 0;
    if (!get_file_contents(filename, contents, modification_time)) {
        
        throw V8CompilationException(*this, std::string("Could not load file: ") + filename);
    }

    return compile(contents);
}


std::shared_ptr<ScriptHelper> ContextHelper::compile(const std::string javascript_source)
{
    return v8toolkit::scoped_run(isolate, context.Get(isolate), [&](){
    
        // printf("Compiling %s\n", javascript_source.c_str());
        // This catches any errors thrown during script compilation
        v8::TryCatch try_catch(isolate);
    
        v8::Local<v8::String> source =
            v8::String::NewFromUtf8(this->isolate, javascript_source.c_str());

        // Compile the source code.
        v8::MaybeLocal<v8::Script> compiled_script = v8::Script::Compile(context.Get(isolate), source);
        if (compiled_script.IsEmpty()) {
            throw V8CompilationException(isolate, v8::Global<v8::Value>(isolate, try_catch.Exception()));
        }
        return std::shared_ptr<ScriptHelper>(new ScriptHelper(shared_from_this(), compiled_script.ToLocalChecked()));
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

            
            v8::Local<v8::Value> e = try_catch.Exception();
            // print_v8_value_details(e);
            
            
            if(e->IsExternal()) {
                auto anybase = (AnyBase *)v8::External::Cast(*e)->Value();
                auto anyptr_exception_ptr = dynamic_cast<Any<std::exception_ptr> *>(anybase);
                assert(anyptr_exception_ptr); // cannot handle other types at this time TODO: throw some other type of exception if this happens UnknownExceptionException or something
            
                // TODO: Are we leaking a copy of this exception by not cleaning up the exception_ptr ref count?
                std::rethrow_exception(anyptr_exception_ptr->get());
            } else {
                printf("v8 internal exception thrown: %s\n", *v8::String::Utf8Value(e));
                throw V8Exception(isolate, v8::Global<v8::Value>(isolate, e));
            }
        }
        v8::Local<v8::Value> result = maybe_result.ToLocalChecked();
    
        return v8::Global<v8::Value>(isolate, result);
    });
}


v8::Global<v8::Value> ContextHelper::run(const std::string code)
{
    return (*this)([this, code]{
        auto compiled_code = compile(code);
        return compiled_code->run();
    });
}



v8::Global<v8::Value> ContextHelper::run(const v8::Local<v8::Value> value)
{
    return (*this)([this, value]{
        return run(*v8::String::Utf8Value(value));
    });
}


std::future<std::pair<v8::Global<v8::Value>, std::shared_ptr<ScriptHelper>>> 
 ContextHelper::run_async(const std::string code, std::launch launch_policy)
{
    // copy code into the lambda so it isn't lost when this outer function completes
    //   right after creating the async
    return (*this)([this, code, launch_policy]{
        return this->compile(code)->run_async(launch_policy);
    });
}


void ContextHelper::run_detached(const std::string code)
{
    (*this)([this, code]{
        this->compile(code)->run_detached();
    });
}


std::thread ContextHelper::run_thread(const std::string code)
{
    return (*this)([this, code]{
        return this->compile(code)->run_thread();
    });
}


IsolateHelper::IsolateHelper(v8::Isolate * isolate) : isolate(isolate)
{   
    v8toolkit::scoped_run(isolate, [this](auto isolate){
        this->global_object_template.Reset(isolate, v8::ObjectTemplate::New(this->get_isolate()));
    });
}

IsolateHelper::operator v8::Isolate*()
{
    return this->isolate;
}

IsolateHelper::operator v8::Local<v8::ObjectTemplate>()
{
    return this->global_object_template.Get(this->isolate);
}

IsolateHelper & IsolateHelper::add_print(std::function<void(const std::string &)> callback)
{
    (*this)([this, callback](){
        v8toolkit::add_print(isolate, get_object_template(), callback);
    });
    return *this;
}

IsolateHelper & IsolateHelper::add_print()
{
    (*this)([this](){
        v8toolkit::add_print(isolate, get_object_template());
    });
    return *this;
}


void IsolateHelper::add_require(std::vector<std::string> paths)
{
    (*this)([this, paths]{
       v8toolkit::add_require(isolate, get_object_template(), paths);
    });
}

v8::Isolate * IsolateHelper::get_isolate() 
{
    return this->isolate;
}


std::shared_ptr<ContextHelper> IsolateHelper::create_context()
{
    return operator()([this](){
        auto ot = this->get_object_template();
        auto context = v8::Context::New(this->isolate, NULL, ot);
    
        // can't use make_unique since the constructor is private
        auto context_helper = new ContextHelper(shared_from_this(), context);
        return std::unique_ptr<ContextHelper>(context_helper);
    });
}

v8::Local<v8::ObjectTemplate> IsolateHelper::get_object_template()
{
    return global_object_template.Get(isolate);
}

IsolateHelper::~IsolateHelper()
{
#ifdef V8TOOLKIT_JAVASCRIPT_DEBUG
    printf("Deleting isolate helper %p for isolate %p\n", this, this->isolate);
#endif

    // must explicitly Reset this because the isolate will be
    //   explicitly disposed of before the Global is destroyed
    this->global_object_template.Reset();
    
    this->isolate->Dispose();
    printf("End of isolate helper destructor\n");
}

void IsolateHelper::add_assert()
{
    add_function("assert", [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();
        // printf("Asserting: '%s'\n", *v8::String::Utf8Value(info[0]));
        
        // printf("AKA: %s\n",  *v8::String::Utf8Value(info[0]->ToString()));

        v8::TryCatch tc(isolate);
        auto script_maybe = v8::Script::Compile(context, info[0]->ToString());
        if(tc.HasCaught()) {
            // printf("Caught compilation error\n");
            tc.ReThrow();
            return;
        }
        auto script = script_maybe.ToLocalChecked();
        auto result_maybe = script->Run(context);
        if(tc.HasCaught()) {
            // printf("Caught runtime exception\n");
            tc.ReThrow();
            return;
        }
        auto result = result_maybe.ToLocalChecked();
        // print_v8_value_details(result);
        
        bool default_value = false;
        bool assert_result = result->BooleanValue(context).FromMaybe(default_value);
        if (!assert_result) {
            throw V8AssertionException(isolate, std::string("Expression returned false: ") + *v8::String::Utf8Value(info[0]));
        }
        
        // printf("Done in assert\n");
    });
    
    add_function("assert_contents", [this](const v8::FunctionCallbackInfo<v8::Value>& args){
        auto isolate = args.GetIsolate();
        if(args.Length() != 2 || !compare_contents(*this, args[0], args[1])) {
            printf("Throwing v8assertionexception\n");
            throw V8AssertionException(*this, std::string("Data structures do not contain the same contents: ")+ stringify_value(isolate, args[0]).c_str() + " " + stringify_value(isolate, args[1]));
        }
    });
}


void PlatformHelper::init(int argc, char ** argv) 
{
    assert(!initialized);
    process_v8_flags(argc, argv);
    
    // Initialize V8.
    v8::V8::InitializeICU();
    
    // startup data is in the current directory
    // TODO: testing how this interacts with lib_nosnapshot.o
    
    // if being built for snapshot use, must call this, otherwise must not call this
#ifdef USE_SNAPSHOTS
    v8::V8::InitializeExternalStartupData(argv[0]);
#endif
    
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

    // can't use make_shared since the constructor is private
    auto isolate_helper = new IsolateHelper(v8::Isolate::New(create_params));
    return std::shared_ptr<IsolateHelper>(isolate_helper);
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
// auto context =   javascript_engine->get_local(javascript_engine->get_context());
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

