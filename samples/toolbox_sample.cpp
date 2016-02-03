#include <string>
#include <fstream>


#define USE_BOOST
#include "v8toolkit.h"
using namespace v8toolkit;


void print_maybe_value(v8::MaybeLocal<v8::Value> maybe_value) 
{
    if (maybe_value.IsEmpty()) {
        printf("Maybe value was empty\n");
    } else {
        auto local = maybe_value.ToLocalChecked();
        v8::String::Utf8Value utf8(local);
        printf("Maybe value: '%s'\n", *utf8);
    }
}

class Point {
public:
    Point(){instance_count++;}
    ~Point(){instance_count--;}
    
    static int get_instance_count(){
        printf("Point::get_instance_count: %d\n", Point::instance_count);
        return instance_count;
    }
    static int instance_count;
    
};

int Point::instance_count = 0;


int foo(int i){return i;}
void bar(){}

int main(int argc, char* argv[]) 
{

    // parse out any v8-specific command line flags
    // process_v8_flags(argc, argv);
    // expose_gc(); // force garbage collection to be exposed even if no command line parameter for it
        
    // Initialize V8.  q
    v8::V8::InitializeICU();
#ifdef USE_SNAPSHOTS
    v8::V8::InitializeExternalStartupData(argv[0]);
#endif
    v8::Platform* platform = v8::platform::CreateDefaultPlatform();
    v8::V8::InitializePlatform(platform);
    v8::V8::Initialize();

    // Create a new Isolate and make it the current one.
    ArrayBufferAllocator allocator;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = &allocator;
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    
    // runs the following code in an isolate and handle scope (no context created yet)
    scoped_run(isolate, [isolate](){
        v8::Local<v8::ObjectTemplate> global_templ = v8::ObjectTemplate::New(isolate);
        add_print(isolate, global_templ);
        std::vector<std::string> paths = {"./"};
        add_require(isolate, global_templ, paths);
        add_module_list(isolate, global_templ);
        
        add_function(isolate, global_templ, "foo", &foo);
        add_function(isolate, global_templ, "bar", &bar);
        Point p;
        add_function(isolate, global_templ, "point_instance_count", &Point::get_instance_count);
        
        int i = 42;
        expose_variable(isolate, global_templ, "exposed_variable", i);
        expose_variable_readonly(isolate, global_templ, "exposed_variable_readonly", i);
        
        int lambda_function_int = 1;
        add_function(isolate, global_templ, "lambda_function", [lambda_function_int](int j)->int{return lambda_function_int + j;});
        
        v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global_templ);

        
        
        // runs the following code in an isolate, handle, and context scope
        scoped_run(isolate, context, [isolate, context](){
            // Create a string containing the JavaScript source code.
            auto js_code = get_file_contents("toolbox_sample.js");
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, js_code.c_str(),
                                    v8::NewStringType::kNormal).ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();


            auto result = script->Run(context);
            print_maybe_value(result);
            
        });
    });

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete platform;
    return 0;
}
