#include <iostream>
#include <fstream>

#include <stdio.h>

#include "v8_class_wrapper.hpp"


// bog standard allocator code from V8 Docs
class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  virtual void* Allocate(size_t length) {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  virtual void* AllocateUninitialized(size_t length) { return malloc(length); }
  virtual void Free(void* data, size_t) { free(data); }
};


// helper for testing code, not a part of the library
// read the contents of the file and return it as a std::string
std::string get_file_contents(const char *filename)
{
  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}


struct Foo {
	
	int i = 42;
	
};

// random sample class for wrapping - not actually a part of the library
class Point {
public:
	Point() : x_(69), y_(69) {printf("created Point\n");}
	Point(int x, int y) : x_(x), y_(y) { printf("created Point with 2 ints\n");}
	Point(const Point & p) { assert(false); /* This is to help make sure none of the helpers are creating copies */ }
	~Point(){printf("****Point destructor called on %p\n", this);}
	int x_, y_;
	int thing(int z, char * zz){printf("In Point::Thing with this %p x: %d y: %d and input value %d %s\n", this, this->x_, this->y_, z, zz); return z*2;}
	int overloaded_method(char * foo){return 0;}
	int overloaded_method(int foo){return 1;}
	const char * stringthing() {return "hello";}
	void void_func() {}
	
	// returns a new point object that should be managed by V8 GC
	Point * make_point(){return new Point();}
	
	Foo f;
	Foo & get_foo(){return f;}
};

struct Line {

	Point p;
    Point & get_point(){return this->p;}

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



int main(int argc, char* argv[]) {
	


	// Initialize V8.
	v8::V8::InitializeICU();
	v8::V8::InitializeExternalStartupData(argv[0]);
	v8::Platform* platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(platform);
	v8::V8::Initialize();

	// Create a new Isolate and make it the current one.
	ArrayBufferAllocator allocator;
	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = &allocator;
	v8::Isolate* isolate = v8::Isolate::New(create_params);
	{
		v8::Isolate::Scope isolate_scope(isolate);

		// Create a stack-allocated handle scope.
		v8::HandleScope handle_scope(isolate);

		// how to expose global variables as javascript variables "x" and "y"
		// global_templ->SetAccessor(String::NewFromUtf8(isolate, "x"), XGetter, XSetter);
		// global_templ->SetAccessor(String::NewFromUtf8(isolate, "y"), YGetter, YSetter);

		// wrap the constructor and add it to the global template
		// Local<FunctionTemplate> ft = FunctionTemplate::New(isolate, create);
		v8::Local<v8::ObjectTemplate> global_templ = v8::ObjectTemplate::New(isolate);

		// // add the function "four()" to javascript
		// global_templ->Set(v8::String::NewFromUtf8(isolate, "four"), FunctionTemplate::New(isolate, four));

		// make the Point constructor function available to JS
		auto & wrapped_point = V8ClassWrapper<Point>::get_instance(isolate);
		wrapped_point.add_constructor("Point", global_templ);
		wrapped_point.add_constructor("SameAsPoint", global_templ); // in case you want to have the same constructor in two places
		wrapped_point.add_constructor<int,int>("Pii", global_templ);
		wrapped_point.add_method(&Point::thing, "thing");

		// overloaded functions can be individually addressed, but they can't be the same name to javascript
		//   at least not without some serious finagling of storing a mapping between a singlne name and
		//   multiple function templates as well as some sort of "closeness" function for determining
		//   which primitive type parameters most closely match the javascript values provided
		wrapped_point.add_method<int (Point::*)(char *)>(&Point::overloaded_method, "overloaded_method1");
		wrapped_point.add_method<int (Point::*)(int)>(&Point::overloaded_method, "overloaded_method2");
		wrapped_point.add_method(&Point::make_point, "make_point");

		wrapped_point.add_method(&Point::stringthing, "stringthing").add_method(&Point::void_func, "void_func");
		wrapped_point.add_member(&Point::x_, "x");
		wrapped_point.add_member(&Point::y_, "y");
		wrapped_point.add_method(&Point::get_foo, "get_foo");
		
		
		auto & wrapped_line = V8ClassWrapper<Line>::get_instance(isolate);
		wrapped_line.add_constructor("Line", global_templ);
		wrapped_line.add_method(&Line::get_point, "get_point");
		
		auto & wrapped_foo = V8ClassWrapper<Foo>::get_instance(isolate);
		wrapped_foo.add_member(&Foo::i, "i");
		

		v8::Local<v8::Context> context = v8::Context::New(isolate, NULL, global_templ);
		v8::Context::Scope context_scope_x(context);


		// Create a string containing the JavaScript source code.
		auto js_code = get_file_contents("code.js");
		v8::Local<v8::String> source =
		    v8::String::NewFromUtf8(isolate, js_code.c_str(),
		                        v8::NewStringType::kNormal).ToLocalChecked();

		// Compile the source code.
		v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();


		auto result = script->Run(context);
		print_maybe_value(result);
		


	}

	// Dispose the isolate and tear down V8.
	isolate->Dispose();
	v8::V8::Dispose();
	v8::V8::ShutdownPlatform();
	delete platform;
	return 0;
}








	// an Isolate is a V8 instance where multiple applications can run at the same time, but only 
	//   on thread can be running an Isolate at a time.  

	// an context represents the resources needed to run a javascript program
	//   if a program monkey patches core javascript functionality in one context it won't be 
	//   visible to another context
	//   Local<Context> context = Context::New(isolate);
	//   A context has a global object template, but function templates can be added to it

	// A handle is a reference to a javascript object and while active will stop the object from being 
	//   garbage collected
	//   handles exist within a stack-only allocated handle scope. (cannot new() a handle scope) 
	//   UniquePersistent handle is like a unique_ptr
	//   Persistent handle is must be released manually with ::Reset() method

	// EscapableHandleScope lets you return a handle scope created inside a function, otherwise
	//   all handles created in that function will be destroyed before a value is returned
	//   Return with: return handle_scope.Escape(array);

	// Templates allow c++ functions and objects to be made visible in javascript.
	//   templates are created within a context and must be created in each context they are to be used in
	//   Templates have accessors and interceptors
	//      accessors are tied to specific field names
	//      interceptors are called on ALL field name gets/sets (either by name foo.bar or by index as in foo[2])

	// Templates can have prototype templates to simulate prototypical inheritance


