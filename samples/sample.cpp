#include <iostream>
#include <fstream>
#include <assert.h>
#include <stdio.h>

#include "v8_class_wrapper.h"

using namespace v8toolkit;
using namespace std;

#define SAMPLE_DEBUG true

struct Foo {
    Foo(){if (SAMPLE_DEBUG) printf("Created Foo %p (default constructor)\n", this);}
    Foo(const Foo &){if (SAMPLE_DEBUG) printf("Foo copy constructor\n"); assert(allow_copy_constructor);}
    ~Foo(){if (SAMPLE_DEBUG) printf("deleted Foo %p\n", this);}
    int i = 42;
	static bool allow_copy_constructor;
};

bool Foo::allow_copy_constructor = true;

// random sample class for wrapping - not actually a part of the library
class Point {
public:
    Point() : x_(69), y_(69) {instance_count++;}
    Point(int x, int y) : x_(x), y_(y) { instance_count++; if (SAMPLE_DEBUG) printf("created Point with 2 ints\n");}
    Point(const Point & p) {instance_count++; assert(false); /* This is to help make sure none of the helpers are creating copies */ }
    ~Point(){instance_count--;}
    int x_, y_;
    int thing(int z, char * zz){if (SAMPLE_DEBUG) printf("In Point::Thing with this %p x: %d y: %d and input value %d %s\n", this, this->x_, this->y_, z, zz); return z*2;}
    int overloaded_method(char * foo){return 0;}
    int overloaded_method(int foo){return 1;}
    const char * stringthing() {return "hello";}
    void void_func() {}
    
    // returns a new point object that should be managed by V8 GC
    Point * make_point(){return new Point();}
    
    // Foo & get_foo(Foo & f)  {return f;}
    
    // Leave this as an r-value return for testing purposes Foo f;
    Foo get_foo() {return Foo();}
    
    static int get_instance_count(){ return instance_count; }
    static int instance_count;
};

int Point::instance_count = 0;


struct Line {
    Line(){if (SAMPLE_DEBUG) printf("Created line %p (default constructor)\n", this);}
    ~Line(){if (SAMPLE_DEBUG) printf("Deleted line %p\n", this);}
    Point p;
    Point & get_point(){return this->p;}
    Point get_rvalue_point(){return Point();}
    void some_method(int){}
    void throw_exception(){throw std::exception();}
};



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

void some_function(int){}

int main(int argc, char* argv[]) 
{

    // parse out any v8-specific command line flags
    process_v8_flags(argc, argv);
    expose_gc(); // force garbage collection to be exposed even if no command line parameter for it
        
    // Initialize V8.
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
    {
        scoped_run(isolate, [&](){
            // how to expose global variables as javascript variables "x" and "y"
            // global_templ->SetAccessor(String::NewFromUtf8(isolate, "x"), XGetter, XSetter);
            // global_templ->SetAccessor(String::NewFromUtf8(isolate, "y"), YGetter, YSetter);

            // wrap the constructor and add it to the global template
            // Local<FunctionTemplate> ft = FunctionTemplate::New(isolate, create);
            v8::Local<v8::ObjectTemplate> global_templ = v8::ObjectTemplate::New(isolate);
        
            add_print(isolate, global_templ);
            add_assert(isolate, global_templ);
			
            add_function(isolate, global_templ, "some_function", some_function);
            add_function(isolate, global_templ, "throw_exception", [](){throw std::exception();});

            // // add the function "four()" to javascript
            // global_templ->Set(v8::String::NewFromUtf8(isolate, "four"), FunctionTemplate::New(isolate, four));

            // make the Point constructor function available to JS
            auto & wrapped_point = V8ClassWrapper<Point>::get_instance(isolate);
            wrapped_point.add_method("thing", &Point::thing);
            add_function(isolate, global_templ, "point_instance_count", &Point::get_instance_count);
        

            // overloaded functions can be individually addressed, but they can't be the same name to javascript
            //   at least not without some serious finagling of storing a mapping between a singlne name and
            //   multiple function templates as well as some sort of "closeness" function for determining
            //   which primitive type parameters most closely match the javascript values provided
            wrapped_point.add_method<int, char*>("overloaded_method1", &Point::overloaded_method);
            wrapped_point.add_method<int, int>("overloaded_method2", &Point::overloaded_method);
            wrapped_point.add_method("make_point", &Point::make_point);

            wrapped_point.add_method("stringthing", &Point::stringthing).add_method("void_func", &Point::void_func);
            wrapped_point.add_member("x", &Point::x_);
            wrapped_point.add_member("y", &Point::y_);
        
            // if you register a function that returns an r-value, a copy will be made using the copy constsructor
            wrapped_point.add_method("get_foo", &Point::get_foo).finalize();
            
            // objects created from constructors won't have members/methods added after the constructor is added
            wrapped_point.add_constructor("Point", global_templ);
            wrapped_point.add_constructor<int,int>("Pii", global_templ);
            
            
        
            auto & wrapped_line = V8ClassWrapper<Line>::get_instance(isolate);
            wrapped_line.add_method("get_point", &Line::get_point);
            wrapped_line.add_method("get_rvalue_point", &Line::get_rvalue_point);
            wrapped_line.add_member("p", &Line::p);
            wrapped_line.add_method("some_method", &Line::some_method).add_method("throw_exception", &Line::throw_exception).finalize();
            
            wrapped_line.add_constructor("Line", global_templ);
            
        
            auto & wrapped_foo = V8ClassWrapper<Foo>::get_instance(isolate);
            wrapped_foo.add_member("i", &Foo::i).finalize();
        
            v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global_templ);
            v8::Context::Scope context_scope_x(context);
            
            std::string js_code;
            if(!get_file_contents("code.js", js_code)) {
                assert(false);
            }
			
            v8::Local<v8::String> source =
                v8::String::NewFromUtf8(isolate, js_code.c_str(),
                                    v8::NewStringType::kNormal).ToLocalChecked();

            // Compile the source code.
            v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();

            auto result = script->Run(context);
            print_maybe_value(result);

            

			// calling a function with too few parameters throws
            v8::Local<v8::Script> script3 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"some_function();")).ToLocalChecked();
            v8::TryCatch tc(isolate);
            (void)script3->Run(context);
            assert(tc.HasCaught());

			// calling a method with too few parameters throws
            v8::Local<v8::Script> script4 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"l=new Line();l.some_method();")).ToLocalChecked();
            v8::TryCatch tc2(isolate);
            (void)script4->Run(context);
            assert(tc2.HasCaught());

            v8::Local<v8::Script> script5 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"throw_exception();")).ToLocalChecked();
            v8::TryCatch tc3(isolate);
            (void)script5->Run(context);
            assert(tc3.HasCaught());

            // printf("Checking that calling a method that throws a c++ exception has its exception wrapped for V8\n");
            v8::Local<v8::Script> script6 = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"l=new Line();l.throw_exception();")).ToLocalChecked();
            v8::TryCatch tc4(isolate);
            (void)script6->Run(context);
            assert(tc4.HasCaught());

        });
        
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete platform;
    return 0;
}

