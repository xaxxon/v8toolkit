#include <fstream>
#include <memory>

#include "javascript.h"

namespace v8toolkit {

Context::Context(std::shared_ptr<Isolate> isolate_helper, v8::Local<v8::Context> context) : 
    isolate_helper(isolate_helper), isolate(isolate_helper->get_isolate()), context(v8::Global<v8::Context>(isolate, context)) 
{}

void Context::shutdown() {
    this->scripts.clear();
    this->shutting_down = true;
}



v8::Local<v8::Context> Context::get_context(){
    return context.Get(isolate);
}


v8::Isolate * Context::get_isolate() 
{
    return this->isolate;
}


std::shared_ptr<Isolate> Context::get_isolate_helper()
{
    return this->isolate_helper;
}

v8::Local<v8::Value> Context::json(const std::string & json) {
    return this->isolate_helper->json(json);
}


Context::~Context() { }


std::shared_ptr<Script> Context::compile_from_file(const std::string & filename)
{
    std::string contents;
    time_t modification_time = 0;
    if (!get_file_contents(filename, contents, modification_time)) {
        
        throw V8CompilationException(*this, std::string("Could not load file: ") + filename);
    }

    return compile(contents, filename);
}


std::shared_ptr<Script> Context::compile(const std::string & javascript_source, const std::string & filename)
{
    GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);
    
    // printf("Compiling %s\n", javascript_source.c_str());
    // This catches any errors thrown during script compilation
    v8::TryCatch try_catch(isolate);
    
    v8::Local<v8::String> source =
	v8::String::NewFromUtf8(this->isolate, javascript_source.c_str());
    
    // Compile the source code.
    auto script_origin = std::make_unique<v8::ScriptOrigin>(v8::String::NewFromUtf8(isolate, filename.c_str()));
    v8::MaybeLocal<v8::Script> compiled_script = v8::Script::Compile(context.Get(isolate), source,  script_origin.get());
    if (compiled_script.IsEmpty()) {
        throw V8CompilationException(isolate, v8::Global<v8::Value>(isolate, try_catch.Exception()));
    }
    return std::shared_ptr<Script>(new Script(shared_from_this(), compiled_script.ToLocalChecked(), std::move(script_origin)));
}


v8::Global<v8::Value> Context::run(const v8::Global<v8::Script> & script)
{
    GLOBAL_CONTEXT_SCOPED_RUN(isolate, context);

    // This catches any errors thrown during script compilation
    v8::TryCatch try_catch(isolate);
    // auto local_script = this->get_local(script);
    auto local_script = v8::Local<v8::Script>::New(isolate, script);
    auto maybe_result = local_script->Run(context.Get(isolate));
    if (try_catch.HasCaught()) {
	printf("Context::run threw exception - about to print details:\n");
	ReportException(isolate, &try_catch);
    } else {
	printf("Context::run ran without throwing exception\n");
    }

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
}


v8::Global<v8::Value> Context::run(const std::string & source)
{
    return (*this)([this, source]{
        auto compiled_code = compile(source);
        return compiled_code->run();
    });
}



v8::Global<v8::Value> Context::run(const v8::Local<v8::Value> value)
{
    return (*this)([this, value]{
        return run(*v8::String::Utf8Value(value));
    });
}


v8::Global<v8::Value> Context::run_from_file(const std::string & filename)
{
	return compile_from_file(filename)->run();
}

std::future<std::pair<v8::Global<v8::Value>, std::shared_ptr<Script>>> 
Context::run_async(const std::string & source, std::launch launch_policy)
{
    // copy code into the lambda so it isn't lost when this outer function completes
    //   right after creating the async
    return (*this)([this, source, launch_policy]{
        return this->compile(source)->run_async(launch_policy);
    });
}


void Context::run_detached(const std::string & source)
{
    (*this)([this, source]{
        this->compile(source)->run_detached();
    });
}


std::thread Context::run_thread(const std::string & source)
{
    return (*this)([this, source]{
        return this->compile(source)->run_thread();
    });
}


Isolate::Isolate(v8::Isolate * isolate) : isolate(isolate)
{   
    v8toolkit::scoped_run(isolate, [this](v8::Isolate * isolate)->void{
        this->global_object_template.Reset(isolate, v8::ObjectTemplate::New(this->get_isolate()));
    });
}

Isolate::operator v8::Isolate*()
{
    return this->isolate;
}

Isolate::operator v8::Local<v8::ObjectTemplate>()
{
    return this->global_object_template.Get(this->isolate);
}

Isolate & Isolate::add_print(std::function<void(const std::string &)> callback)
{
    (*this)([this, callback](){
        v8toolkit::add_print(isolate, get_object_template(), callback);
    });
    return *this;
}

Isolate & Isolate::add_print()
{
    (*this)([this](){
        v8toolkit::add_print(isolate, get_object_template());
    });
    return *this;
}


void Isolate::add_require(std::vector<std::string> paths)
{
    (*this)([this, paths]{
       v8toolkit::add_require(isolate, get_object_template(), paths);
    });
}

v8::Isolate * Isolate::get_isolate() 
{
    return this->isolate;
}


std::shared_ptr<Context> Isolate::create_context()
{
    ISOLATE_SCOPED_RUN(this->isolate);
    v8::TryCatch tc(this->isolate);

    auto ot = this->get_object_template();
    auto context = v8::Context::New(this->isolate, NULL, ot);


    if (tc.HasCaught() || context.IsEmpty()) {
	    throw V8ExecutionException(this->isolate, tc);
    }

    
    // can't use make_shared since the constructor is private
    auto context_helper = new Context(shared_from_this(), context);

    return std::shared_ptr<Context>(context_helper);
	
}

v8::Local<v8::ObjectTemplate> Isolate::get_object_template()
{
    return global_object_template.Get(isolate);
}

Isolate::~Isolate()
{
#ifdef V8TOOLKIT_JAVASCRIPT_DEBUG
    printf("Deleting isolate helper %p for isolate %p\n", this, this->isolate);
#endif

    // must explicitly Reset this because the isolate will be
    //   explicitly disposed of before the Global is destroyed
    this->global_object_template.Reset();
    this->isolate->Dispose();
}

void Isolate::add_assert()
{
    add_function("assert", [](const v8::FunctionCallbackInfo<v8::Value>& info) {
        auto isolate = info.GetIsolate();
        auto context = isolate->GetCurrentContext();

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

        bool default_value = false;
        bool assert_result = result->BooleanValue(context).FromMaybe(default_value);
        if (!assert_result) {
            throw V8AssertionException(isolate, std::string("Expression returned false: ") + *v8::String::Utf8Value(info[0]));
        }
        
    });
    
    add_function("assert_contents", [this](const v8::FunctionCallbackInfo<v8::Value>& args){
        auto isolate = args.GetIsolate();
        if(args.Length() != 2 || !compare_contents(*this, args[0], args[1])) {
            // printf("Throwing v8assertionexception\n");
            throw V8AssertionException(*this, std::string("Data structures do not contain the same contents: ")+ stringify_value(isolate, args[0]).c_str() + " " + stringify_value(isolate, args[1]));
        }
    });
}


void Platform::expose_gc() {
    assert(!Platform::initialized);
    expose_gc_value = true;
}


void Platform::set_max_memory(int new_memory_size_in_mb) {
    Platform::memory_size_in_mb = new_memory_size_in_mb;
}


void Platform::expose_debug_as(const std::string & debug_object_name) {
    assert(!Platform::initialized);
    expose_debug_value = true;
    expose_debug_name = debug_object_name;    
}

    

void Platform::init(int argc, char ** argv) 
{
    assert(!initialized);
    process_v8_flags(argc, argv);

    if (expose_gc_value) {
	expose_gc();
    }
    if (expose_debug_value) {
	expose_debug_as(expose_debug_name);
    }
    
    // Initialize V8.
    v8::V8::InitializeICU();
    
    // startup data is in the current directory
    
    // if being built for snapshot use, must call this, otherwise must not call this
#ifdef USE_SNAPSHOTS
    v8::V8::InitializeExternalStartupData(argv[0]);
#endif
    
    Platform::platform = std::unique_ptr<v8::Platform>(v8::platform::CreateDefaultPlatform());
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    
    initialized = true;
}

void Platform::cleanup()
{

    // Dispose the isolate and tear down V8.
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    
    platform.release();
};

std::shared_ptr<Isolate> Platform::create_isolate()
{
    assert(initialized);
    v8::Isolate::CreateParams create_params;
    if (Platform::memory_size_in_mb > 0) {
        create_params.constraints.set_max_old_space_size(Platform::memory_size_in_mb);
    }
    create_params.array_buffer_allocator = (v8::ArrayBuffer::Allocator *) &Platform::allocator;

    // can't use make_shared since the constructor is private
    auto isolate_helper = new Isolate(v8::Isolate::New(create_params));
    return std::shared_ptr<Isolate>(isolate_helper);
}


Script::Script(std::shared_ptr<Context> context_helper, v8::Local<v8::Script> script, std::unique_ptr<v8::ScriptOrigin> script_origin) :
    context_helper(context_helper),
    isolate(*context_helper),
    script(v8::Global<v8::Script>(isolate, script)),
    script_origin(std::move(script_origin))
{}

std::thread Script::run_thread()
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


void Script::run_detached(){
    run_thread().detach();
}



    bool Platform::initialized = false;
std::unique_ptr<v8::Platform> Platform::platform;
v8toolkit::ArrayBufferAllocator Platform::allocator;

bool Platform::expose_gc_value = false;
bool Platform::expose_debug_value = false;
std::string Platform::expose_debug_name = "";
int Platform::memory_size_in_mb = -1;
    
} // end v8toolkit namespace
