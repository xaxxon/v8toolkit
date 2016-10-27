#include <iostream>
#include <fstream>
#include <assert.h>
#include <stdio.h>

#include <fmt/format.h>
#define V8TOOLKIT_ENABLE_EASTL_SUPPORT
#define EA_HAVE_CPP11_INITIALIZER_LIST 1
#include <EASTL/fixed_string.h>

#include "v8_class_wrapper.h"

using namespace v8toolkit;
using namespace v8toolkit::literals;
using namespace std;



// EA STL requirements
void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}


#define SAMPLE_DEBUG true

struct FooParent {
    // must be polymorphic for most derived type to be returned
    virtual ~FooParent() {};
};

struct Foo : public FooParent {
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
    Point & operator=(const Point &){assert(false);}
    Point(Point&&){cerr<<"Point move constructor called" << endl;}
    Point & operator=(Point&&){cerr << "Point move assignment called" << endl; return *this;}
    ~Point(){instance_count--;}
    int x_, y_;
    int thing(int z, char * zz){if (SAMPLE_DEBUG) printf("In Point::Thing with this %p x: %d y: %d and input value %d %s\n", this, this->x_, this->y_, z, zz); return z*2;}
    int overloaded_method(char * foo){return 0;}
    int overloaded_method(int foo){return 1;}
    const char * stringthing() {return "hello";}
    void void_func() {}
    eastl::fixed_string<char, 32, true, eastl::allocator> fixed_string;

    int operator()(int x) {
	return x + x_;
    }
    
    // returns a new point object that should be managed by V8 GC
    Point * make_point(){return new Point();}
    
    // Foo & get_foo(Foo & f)  {return f;}
    
    // Leave this as an r-value return for testing purposes Foo f;
    Foo get_foo() {return Foo();}
    
    static int get_instance_count(){ return instance_count; }
    static int instance_count;

    // test for bug where using the same static method name on two classes
    //   causes duplicate attribute error
    static void static_method_same_name(){};

    int operator[](uint32_t index) {return index;}
    std::string operator[](std::string const & str) {return str;}
};

int Point::instance_count = 0;


struct PointSubclass : public Point {
    static void static_method_same_name(){};
};

struct PointSubclass2 : public Point {
    static void static_method_same_name(){};
};


struct Line {
    Line(){if (SAMPLE_DEBUG) printf("Created line %p (default constructor)\n", this);}
    Line(const Line &) = delete; // wrapper cannot require copy constructor
    Line(Line &&) = delete; // wrapper cannot require move constructor
    ~Line(){if (SAMPLE_DEBUG) printf("Deleted line %p\n", this);}
    Point p;
    Point & get_point(){return this->p;}
    Point get_rvalue_point(){return Point();}
    void some_method(int){}
    std::string echo(const std::string & input){printf("In echo"); return input;}
    void throw_exception(){throw std::exception();}
    static int static_method(float){return 42;}
    void takes_function(std::function<Foo&()>){}
    void takes_const_ref_fundamental(const int & i) {}
    void takes_ref_fundamental(int & i) {}

    void take_point(Point && point){p = std::move(point);}
    void take_map(map<string, int> && new_map){map<string, int> my_map(std::move(new_map));}

    void take_unique(unique_ptr<Line> upl){unique_ptr<Line> line(std::move(upl));}
    void take_unique_ref(unique_ptr<Line> && upl){unique_ptr<Line> line(std::move(upl));}

    void take_unique_int(unique_ptr<int> upl){unique_ptr<int> line(std::move(upl));}
    void take_unique_int_ref(unique_ptr<int> && upl){unique_ptr<int> line(std::move(upl));}


    static void static_method_same_name(){};

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

            bool got_duplicate_name_exception = false;


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
            V8ClassWrapper<Point> & wrapped_point = V8ClassWrapper<Point>::get_instance(isolate);
            wrapped_point.add_static_method("static_method_same_name", &Point::static_method_same_name);
            wrapped_point.add_method("thing", &Point::thing);
	        wrapped_point.make_callable(&Point::operator());
            add_function(isolate, global_templ, "point_instance_count", &Point::get_instance_count);
        

            // overloaded functions can be individually addressed, but they can't be the same name to javascript
            //   at least not without some serious finagling of storing a mapping between a singlne name and
            //   multiple function templates as well as some sort of "closeness" function for determining
            //   which primitive type parameters most closely match the javascript values provided
            wrapped_point.add_method<int, Point, char*>("overloaded_method1", &Point::overloaded_method);
            wrapped_point.add_method<int, Point, int>("overloaded_method2", &Point::overloaded_method);
            wrapped_point.add_method("make_point", &Point::make_point);

            wrapped_point.add_method("stringthing", &Point::stringthing).add_method("void_func", &Point::void_func);
            wrapped_point.add_member("x", &Point::x_);
            wrapped_point.add_member("fixed_string", &Point::fixed_string);
            int changed_x = 0;
	                wrapped_point.register_callback([&changed_x](v8::Isolate * isolate,
                                                         v8::Local<v8::Object> & object,
                                                         const std::string & property_name,
                                                         const v8::Local<v8::Value> & value) {
                auto point = CastToNative<Point*>()(isolate, object);
                printf("%d,%d: property change callback: %s => %s\n",point->x_, point->y_, property_name.c_str(), *v8::String::Utf8Value(value));
                changed_x++;
            });

            wrapped_point.add_member("y", &Point::y_);
            wrapped_point.add_index_getter([](uint32_t index, v8::PropertyCallbackInfo<v8::Value> const & info){
                printf("index getter: %d\n", index);
                info.GetReturnValue().Set(index);
            });
            // wrapped_point.add_named_property_getter<std::string>(std::bind<std::string(Point::*)(std::string const &)>(&Point::operator[], std::placeholders::_1, std::placeholders::_2));
            wrapped_point.add_named_property_getter(&Point::operator[]);

            got_duplicate_name_exception = false;
            try {
                wrapped_point.add_member("y", &Point::y_);
            } catch (DuplicateNameException &) {
                got_duplicate_name_exception = true;
            }
            assert(got_duplicate_name_exception);


            // if you register a function that returns an r-value, a copy will be made using the copy constsructor
            wrapped_point.add_method("get_foo", &Point::get_foo);
            wrapped_point.set_compatible_types<PointSubclass, PointSubclass2>();
	        wrapped_point.finalize();

            // objects created from constructors won't have members/methods added after the constructor is added
            wrapped_point.add_constructor("Point", global_templ);
            wrapped_point.add_constructor<int,int>("Pii", global_templ);



            auto & point_subclass = V8ClassWrapper<PointSubclass>::get_instance(isolate);
            point_subclass.add_static_method("static_method_same_name", &PointSubclass::static_method_same_name);
            point_subclass.set_parent_type<Point>();
            point_subclass.finalize();
            point_subclass.add_constructor<>("PointSubclass", global_templ);


            auto & point_subclass2 = V8ClassWrapper<PointSubclass2>::get_instance(isolate);
            point_subclass2.add_static_method("static_method_same_name", &PointSubclass2::static_method_same_name);
            point_subclass2.set_parent_type<Point>();
            point_subclass2.finalize();
            point_subclass2.add_constructor<>("PointSubclass2", global_templ);



            V8ClassWrapper<Line> & wrapped_line = V8ClassWrapper<Line>::get_instance(isolate);
            wrapped_line.add_static_method("static_method_same_name", &Line::static_method_same_name);
            wrapped_line.add_method("get_point", &Line::get_point);
            wrapped_line.add_method("get_rvalue_point", &Line::get_rvalue_point);
            wrapped_line.add_member("p", &Line::p);
            wrapped_line.add_method("some_method", &Line::some_method).add_method("throw_exception", &Line::throw_exception);
            wrapped_line.add_static_method("static_method", &Line::static_method);
            wrapped_line.add_static_method("static_lambda", [](){return 43;});
            wrapped_line.add_method("fake_method", [](Line * line){
                printf("HI");
                return line->echo("line echo called from fake_method");
            });

            wrapped_line.add_method("takes_function", &Line::takes_function);
            wrapped_line.add_method("takes_const_ref_fundamental", &Line::takes_const_ref_fundamental);
            wrapped_line.add_method("takes_ref_fundamental", &Line::takes_ref_fundamental);
            wrapped_line.add_method("take_point", &Line::take_point);
            wrapped_line.add_method("take_map", &Line::take_map);
            wrapped_line.add_method("take_unique", &Line::take_unique);
            wrapped_line.add_method("take_unique_ref", &Line::take_unique_ref);
            wrapped_line.add_method("take_unique_int", &Line::take_unique_int);
            wrapped_line.add_method("take_unique_int_ref", &Line::take_unique_int_ref);

            got_duplicate_name_exception = false;
            try {
                wrapped_line.add_method("takes_ref_fundamental", &Line::takes_ref_fundamental);
            } catch (DuplicateNameException &) {
                got_duplicate_name_exception = true;
            }
            assert(got_duplicate_name_exception);

            got_duplicate_name_exception = false;
            try {
                wrapped_line.add_static_method("static_method", &Line::static_method);
            } catch (DuplicateNameException &) {
                got_duplicate_name_exception = true;
            }
            assert(got_duplicate_name_exception);


            got_duplicate_name_exception = false;
            try {
                wrapped_line.add_static_method("static_lambda", [](){return 43;});
            } catch (DuplicateNameException &) {
                got_duplicate_name_exception = true;
            }
            assert(got_duplicate_name_exception);



            wrapped_line.finalize();
            
            wrapped_line.add_constructor("Line", global_templ);
            

            auto & wrapped_fooparent = V8ClassWrapper<FooParent>::get_instance(isolate);
            wrapped_fooparent.set_compatible_types<Foo>();
            wrapped_fooparent.finalize(true);


            auto & wrapped_foo = V8ClassWrapper<Foo>::get_instance(isolate);
            wrapped_foo.set_parent_type<FooParent>();
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

            v8::Local<v8::Script> script_for_static_method = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"println(Line.static_method(42));")).ToLocalChecked();
            (void)script_for_static_method->Run(context);

            v8::Local<v8::Script> script_for_fake_method = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"l = new Line(); println(l.fake_method());")).ToLocalChecked();
            (void)script_for_fake_method->Run(context);


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

            assert(changed_x == 0);
            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"p=new Pii(1,2);p2=new Pii(3,4); p.x = 2; p2.x = 4")).ToLocalChecked();
            (void)script->Run(context);
            assert(changed_x == 2);
            (void)script->Run(context);
            assert(changed_x == 4);

            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"p = new Point(); l = new Line(); l.take_point(p); l.take_map({a:5, b: 6});")).ToLocalChecked();
            (void)script->Run(context);
            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"p = new Point(); println(p[4]);")).ToLocalChecked();
            (void)script->Run(context);
            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"p = new Point(); println(p['four']);")).ToLocalChecked();
            (void)script->Run(context);

            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"p = new Point(); l = new Line(); l.take_point(p); l.take_map({a:5, b: 6});")).ToLocalChecked();
            (void)script->Run(context);

            script = v8::Script::Compile(context, v8::String::NewFromUtf8(isolate,"()=>42.2")).ToLocalChecked();
	    assert(CastToNative<float>()(isolate, script->Run(context).ToLocalChecked()) == 42.2f);




	        Foo most_derived_foo_test;
            v8::Local<v8::Object> most_derived_fooparent_js_object =
                wrapped_fooparent.wrap_existing_cpp_object<DestructorBehavior_LeaveAlone>(context, &most_derived_foo_test);

            // even though this is wrapped as a FooParent that doesn't have an 'i', it should actually be wrapped
            //   as the most derived type of the actual cpp object inside, which is a Foo, which does have an 'i'
            fprintf(stderr, "Testing most derived type return\n");
            fprintf(stderr, "%s\n", stringify_value(isolate, most_derived_fooparent_js_object).c_str());
            assert(!most_derived_fooparent_js_object->Get(context, "i"_v8).ToLocalChecked()->IsUndefined());
            fprintf(stderr, "Testing completed\n");

        });
        
    }

    // Dispose the isolate and tear down V8.
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::ShutdownPlatform();
    delete platform;
    return 0;
}

