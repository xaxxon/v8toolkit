# v8toolkit
v8toolkit is a library for easily integrating JavaScript with C++.   Just tell v8toolkit what 
parts of your C++ you want it to expose to JavaScript and v8toolkit takes care of the rest -- or you can annotate
your C++ code inline and use the included clang plugin to scan your code and generate the JavaScript bindings
automatically.

## Features
* Default bindings for C++ fundamental types, and most STL containers.
* User-defined type support easy to add.
* Support for simple variables and functions as well as class member functions, static functions, and data members, and enums.
* JavaScript debugging support.  Point Chrome ([or many other debuggers](https://developer.chrome.com/devtools/docs/debugging-clients)) at your application and see your JavaScript source, set breakpoints, inspect and set variables.   
* Automatic generation of JavaScript bindings directly from your C++ source code using a provided plugin for the clang compiler. A stubbed out .js file is also generated with JSDoc comments and type information, directly from your C++ source, for autocompletion in compatible editors while using your C++ types from JavaScript. 
* Understands memory ownership and C++11 `std::unique_ptr` and rvalues.   C++ functions taking/returning rvalue references or `std::unique_ptr` will transfer ownership of the underlying C++ object back and forth between C++ and the JavaScript garbage collector.
* Default parameters for calling C++ functions from JavaScript and not providing enough parameters.  Experimental support in the clang plugin for generating the defaults from the default values in your C++ source.

## Requirements
* Recent version of V8.  The V8 API is constantly evolving and this library tracks recent versions.
* C++17. New language features drastically simplifying v8toolkit's implementation.  Fortunately, clang
supports all major platforms.  (Currently requires clang, as GCC never implemented the C++11 feature
allowing function pointers without external binding as template parameters - https://gcc.gnu.org/bugzilla/show_bug.cgi?id=52036)

#### Doxygen docs available here: http://xaxxon.github.io/v8toolkit/docs/doxygen/html/index.html

## New Feature!
###Auto-generation of PIMPL members.   

Annotate the main class, put a friend on it, and all the 
members of the PIMPL member pointer (subject to the usual rules/exceptions) will be exposed as
part of the main wrapped class.

Additionally, add_member(_readonly) can now expose any variable - just provide it with a function
callback which returns the variable you want exposed when given an object pointer.
   
## Examples

### Your first v8toolkit program

```language-c++
    #include <v8toolkit/javascript.h>
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

### Exposing a C++ class to JavaScript manually

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

### Automatic generation of JavaScript bindings for a C++ class

By annotating your C++ class (note the `V8TOOLKIT_SKIP` in the code below), you can generate the exact same bindings 
as above automatically using the clang plugin.  


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
