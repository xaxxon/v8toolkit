# v8toolkit
v8toolkit is a library to make integrating JavaScript to C++ as painless as possible.   Just tell v8toolkit what 
parts of your C++ you want it to expose to JavaScript and it takes care of the rest.   Or you can annotate
your C++ code with C++ attributes and use the included clang plugin to scan your code and generate the JavaScript bindings
automatically.

## Features
* Type-aware bindings for C++ fundamental types, and a good number of STL containers.
* Straight-forward support for adding custom C++ to JavaScript serializer/deserializers for user-defined types.
* Simple syntax to expose your class to JavaScript including member functions, static functions,
and data members.
* Debugger support via anything supporting the Chrome Debugging Protocol.  Point Chrome at your application
and see your JavaScript source, set breakpoints, inspect and set variables.   
* Automatic generation of JavaScript bindings directly from your C++ source code using a provided plugin for 
the clang compiler using the actual AST generated during compilation.
* Understands memory ownership and C++11 `std::unique_ptr`.   C++ functions taking/returning rvalue references 
or `std::unique_ptr` will transfer ownership of the underlying C++ object back and forth between C++ and the
JavaScript garbage collector.
* Default parameters when calling C++ functions from JavaScript and not providing enough parameters.  There is
experimental support for automatically generating the default values directly from your C++ source code while
using the clang plugin.

## Requirements
* Recent version of V8.  The V8 API is constantly evolving and this library tends to track the recent versions.
* C++17.  C++17 adds features drastically simplifying v8toolkit's heavily templated code.  Fortunately, clang 4.0
supports all major platforms.  

#### Doxygen docs available here: http://xaxxon.github.io/v8toolkit/docs/doxygen/html/index.html

   

## Your First v8toolkit program

```language-c++
    #include "javascript.h"
    using v8toolkit;
   
   int main(int argc, char ** argv) {
      // one-time initialization
      Platform::init(argc, argv, argv[0]); 
      
      // An isolate is the top-level JavaScript container.
      auto isolate = Platform::create_isolate();
      isolate->add_print(); // JavaScript can't print to standard out on its own, so this adds some custom functions
        
      // a context is the environment in which JavaScript code is executed and belongs to an Isolate
      auto context = isolate->create_context();
      
      // prints "Hello JavaScript World!" to standard out.
      context->run("println('Hello JavaScript World!');");
    }
```


Here is an example class and the code required to make the class useable from JavaScript:

```language-c++
    class MyClassToWrap : public v8toolkit::WrappedClassBase {
    public:
        void member_function();
        void member_function_not_exposed();
        
        static void static_method();

        std::string data_member;
        int const const_data_member;
        
        MyClassToWrap(int i);

    };

   void create_class_bindings(v8toolkit::Isolate & isolate) {
        auto & class_wrapper = isolate.wrap_class<MyClassToWrap>();
        class_wrapper.add_function("a_member_function", &MyClassToWrap::a_member_function);
        class_wrapper.add_static_function("static_method", &MyClassToWrap::static_method);
        class_wrapper.add_member<&MyClassToWrap::data_member>("data_member");
        class_wrapper.finalize();
        class_wrapper.add_contructor<int>("MyClassWrapper", *i);
   }

```

Or, you can automatically generate the source code above by annotating the C++ class definition to tell it anything
that shouldn't be exposed, and run the clang plugin over it and then compile the generated code into your project:


```language-c++
    class MyClassToWrap : public v8toolkit::WrappedClassBase {
    public:
        void member_function();
        V8TOOLKIT_SKIP void member_function_not_exposed(); 
        
        static void static_method();

        std::string data_member;
        int const const_data_member;
        
        MyClassToWrap(int i);

    };
```
