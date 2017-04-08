
## Doxygen docs available here: http://xaxxon.github.io/v8toolkit/docs/html/index.html

## New Feature: Default Arguments

Support for specifying default arguments for native calls is now supported and they are
automatically generated from your C++ source when using the ClassParser plugin.

   
## New Feature: Debugging embedded JavaScript from Chrome's javascript debugger.

* Viewing code 
* Add/remove breakpoints
* Step over/into/out
* Notification on breakpoint being hit
* Resuming execution


To debug, you must start chrome with the `--remote-debugging-port=9222`.
Then, go to `http://localhost:9222/devtools/inspector.html?ws=localhost:9002`.

Note the two different ports above.  The first is a local port to serve the debugger from, the second is the port
in the program you wish to debug.

This code will be rewritten using the newly implemented debugging interface in V8, but
until then, the existing code has some of the functionality.


## New Feature: ClassParser - Automatic class binding generator

`class_parser/` directory now contains a clang plugin that can automatically generate bindings for your existing
C++ classes using the actual clang compiler AST data.  From this, it generates `V8ClassWrapper` bindings, 
creates `bidirectional` classes, and creates a javascript 'stub' file for hinting editor
autocompletion.

C++ classes can be annotated to customize exactly how and which elements will be included.

    class V8TOOLKIT_WRAPPED_CLASS MyClassToWrap {
    
        // Constructor automatically wrapped with its own name
        MyClassToWrap();

        void this_function_will_be_wrapped();
        V8TOOLKIT_SKIP void do_not_wrap_this_function();
        
        static void static_methods_work_too();

        std::string data_members_work;
        int const const_data_members_are_wrapped_read_only;
    };


The plugin requires clang, but the generated code will compile on any compiler.


## Tutorial

#### Install git

https://git-scm.com/book/en/v2/Getting-Started-Installing-Git


#### Building V8

Building V8 is not a simple process and changes often, so these instructions may not
be up-to-date.

 * First, you need to install a set of tools for getting and building V8 called "depot tools".

https://www.chromium.org/developers/how-tos/install-depot-tools

 * Then, download V8 via the instructions here: 

https://github.com/v8/v8/wiki/Using%20Git

The official build process documentation is here:

https://github.com/v8/v8/wiki/Building%20with%20GN

and the previous build process (simpler but will stop working eventually) here:

https://github.com/v8/v8/wiki/Building%20with%20Gyp

* The steps I use on each platform are documented here:

[Build V8 for OS X](osx_v8_build.md)

[Build V8 for Windows (Visual Studio)](windows_v8_build.md)

[Build V8 for Linux](linux_v8_build.md)


#### Install the following v8toolkit dependencies

Boost: http://www.boost.org

Websocket library used for communicationg with chrome javascript debugger:
https://github.com/zaphoyd/websocketpp

Text formatting library (faster but less featureful than boost::format):
https://github.com/fmtlib/fmt


#### Build v8toolkit


`git clone https://github.com/xaxxon/v8toolkit.git` 

Create a build directory (anywhere, but I prefer inside the directory v8toolkit was cloned into) and type:
    cmake -DV8_BASE_SHARED_LIB_DIR:PATH=/path/to/v8/library/files -DV8_INCLUDE_DIR:PATH=/path/to/v8/include/ /path/to/v8toolkit

and then type: `make` then `make install` (may need sudo)


#### Your First V8 program

Here is the simplest program you can write to execute some javascript using the v8toolkit library:

    #include "javascript.h"
    using v8toolkit;
    
    int main(int argc, char ** argv) {
        // any v8-specific command-line arguments will be stripped out
        Platform::init(argc, argv, argv[0]); 
        
        // creates a v8toolkit::Isolate which can manage some number of contexts
        auto isolate = Platform::create_isolate();
        
        // javascript doesn't have any way to print to stdout,
        //   so this exposes some handy functions to javascript
        isolate->add_print(); 
        
        // a context represents an actual javascript environment in which code is executed
        auto context = isolate->create_context(); 

        // This actually runs the javascript inside the quotes and
        //   prints to the screen using the print helper function we added above
        context->run("println('Hello JavaScript World!');"); 
    }

Working backwards through the code, a context is the container in which JavaScript code actually runs.   Any changes 
made to globally accessible objects in a context persist and will be seen any other JavaScript run within that same context.

An isolate contains the necessary state for any number of related context objects.  An isolate can also be customized 
so that all contexts created after the customization share that customization.   In the example above, if we made 
another context by calling `isolate->create_context()` a second time, this second context would also have access to 
println().  However, if a second isolate were made, contexts created from it would not have the print helpers unless 
add_print() were also to be called on the second isolate.  Lastly, regardless of how many contexts exist within an 
isolate, only one of them can be active at a time.   If you want multiple threads running JavaScript, you must have 
multiple isolates.

Platform manages the per-process configuration for V8 and is used to create isolates.  It has no interaction with 
your JavaScript execution.  Simply call the static `::init()` method on it once and V8 is ready to go.

##### v8toolkit provides classes wrapping native V8 classes with the same names.  This means you cannot `use namespace` both the v8 and v8toolkit namespaces at the same time or you will get ambiguity errors.

#### Compiler options to build your program

Here are some example command lines for building an application using V8.  In it are some things that will need to be replaced with the actual location on the 
computer being used:

\<PATH_TO_V8_BASE_DIRECTORY> - the directory containing the v8 include directory.   This cannot include the "include/" directory.

\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR> - location where boost/format.hpp is located.  This is likely /usr/local/include and cannot include the "boost/" directory

\<YOUR_PROGRAM_CPP> - your source code

\<PATH_TO_V8TOOLKIT> - path where this library is installed and built and the file libv8toolkit.a exists

Successfully running the commands below will build your program as `./a.out`

On Mac:

    clang++ -std=c++14 -g -I./ -I\<PATH_TO_V8_BASE_DIRECTORY> -g -I\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR>  -Wall -Werror  YOUR_PROGRAM_CPP  
    -L\<PATH_TO_V8_BASE_DIRECTORY>/out/native/  -I\<PATH_TO_V8TOOLKIT> \<PATH_TO_V8TOOLKIT>/libv8toolkit.a -lv8_base -lv8_libbase -lv8_base -lv8_libplatform -lv8_nosnapshot -licudata -licuuc -licui18n  
    
On Linux (tested on Ubuntu Desktop 15.10):

    g++ -std=c++14 -g -I\<PATH_TO_V8TOOLKIT> -I\<PATH_TO_V8_BASE_DIRECTORY>  -I\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR>  -Wall -Werror  \<YOUR_PROGRAM_CPP>
    -L\<PATH_TO_V8_BASE_DIRECTORY>/native/obj.target/tools/gyp/ -L\<PATH_TO_V8_BASE_DIRECTORY>/out/native/obj.target/third_party/icu/  -I\<PATH_TO_V8TOOLKIT>
    \<PATH_TO_V8TOOLKIT>/libv8toolkit.a -lv8_base -lv8_libbase -lv8_base -lv8_libplatform -lv8_nosnapshot -licudata -licuuc -licui18n  -lpthread -licuuc -licudata -ldl
    
On Windows:
    v8toolkit is currently untested on windows, but has worked in the past in Visual Studio.

#### Exposing C++ functions to JavaScript

Simply running pure javascript as in the previous example just isn't interesting, though.  
We want to cross the boundary between JavaScript and C++ seemlessly.  Skipping some of the 
boilerplate in the previous example, here's how to call a custom C++ function from JavaScript.

    // just a normal function, nothing special about it
    int add_numbers(int x, int y) 
    { 
        return x + y; 
    }
    
    auto i = Platform::create_isolate();
    i->add_print();
    
    // adds a function named "js_add_numbers" to each context created from this isolate that calls 
    //   the C++ function add_numbers
    i->add_function("add_numbers", add_numbers);
    auto c = i->create_context();
    
    // prints 9
    c->run("println(add_numbers(4,5));");

`add_function` can take anything a std::function can hold and exposes it to javascript with 
the given name.

#### Exposing global C++ variables to JavaScript

Global variables (as well as class public static variables) can be exposed to javascript, as well.   

    int x = 0;
    i->expose_variable("x", x);
    ...
    c->run("x = 4; println(x);"); // prints 4
    printf("%d\n", x); // also prints 4

Virtually identical to adding a function.  Respects `const`ness of variable.


#### Using STL containers 

Many common STL containers are supported and behave in a fairly intuitive manner.   If the container acts mostly like an array, it is turned into
a JavaScript array when passed back to JavaScript.   If the container acts more like an associatie array
(key/value pairs), it is turned into a plain javascript object.

    std::vector<int> make_three_element_vector(int i, int j, int k) {
        return std::vector{i,j,k};
    }
    
    // name mismatch, but each describes what they do from the user's perspective
    i->add_function("make_three_element_array", make_three_element_vector); 
    ...
    c->run("a=make_three_element_array(2,3,5); println(a[1]);"); // prints 3



You can find which containers are supported as well as add your own here: https://github.com/xaxxon/v8toolkit/blob/master/include/casts.hpp   

There is experimental support for some EASTL types, as well.   To enable it, in your
program before including any v8toolkit headers:

     #define V8TOOLKIT_ENABLE_EASTL_SUPPORT

#### Exposing your C++ class to javascript

Sample C++ class:

    class Person {
    private:
        std::string name;
        int age;
        
    public:
        Animal(std::string name, int age) : name(name), age(age) {}
        virtual speak() const {printf("Hi.");}
        
        // people are very impressionable on what their favorite color is.
        std::string favorite_color = "purple";
        
        std::string get_name(){return name;}

        int set_age(int new_age) {this->age = new_age;}
        int get_age() const {return age;}
    }

To make Person available in javascript, first tell the library it's a class it should know about.
Then tell it what parts of that class it should make available to javascript.  There's no requirement to
make everything available, and unfortunately there's no introspection in C++ to look at the class and make
everything available automatically.

    auto person_wrapper = i->wrap_class<Person>;

    // If you weren't familiar with the syntax for getting a method pointer, now you are.
    person_wrapper.add_method("speak", &Person::speak);
    
    // If you weren't familiar with the syntax for getting a data-member pointer, now you are.
    person_wrapper.add_member("favorite_color", &Person::favorite_color);
    
    // you can chain calls while wrapping your class
    person_wrapper.add_method("get_name", &Person::get_name);

    person_wrapper.add_method("set_age", &Person::set_age);
    person_wrapper.add_method("get_age", &Person::get_age);


    person_wrapper.finalize(); // no more attributes can be added after this point

    // adds a constructor function in JavaScript called "Person"
    person_wrapper.add_constructor<std::string, int>("Person");

    // It's very important to make the context AFTER finalizing the class.  Only contexts made from the same
    //   isolate after the class is wrapped will see your class.
    c = i->create_context();
    c->run("p = new Person();  p.speak(); println(p.get_name(), ' is ', p.get_age(), ' years old'); println('Favorite color: ', p.favorite_color);  ");


Multiple constructors and overloaded functions must be given different JavaScript names.


##### Const User-Defined Types

Across all of v8toolkit, const types are automatically handled appropriately.


##### Creating "Fake" Methods

Fake methods act like member functions, but are actually plain functions which take the 'this'
object explicitly as their first paramter.

    your_class_wrapper.add_method("fake_method_name", [](YourClass * your_class_object, char *){...});


From JavaScript, it looks just like any other method:

    var my_class = new MyClass;
    my_class.fake_method_name("Test"); // the YourClass* parameter is sent automatically

If you want the fake method to be associated with `const YourClass` as well, have the lambda
take a `const YourClass *` instead.


#### Introducing the Script Object

The `Script` object holds compiled JavaScript.

    // returns a compiled script that can be run multiple times without re-compiling
    auto script = context->compile("println('Hello Javascript World!');");
    
    script->run();
    script->run();

That compiled script always runs in the context it was created in.   If you need to run the 
same code in another context, you will need to recompile it there.


#### v8toolkit object lifetimes

The v8toolkit Isolate, Context objects take care of making sure they don't 
go out of scope while they are still needed.

    {
        std::shared_ptr<Script> s;
        {
            // Platform is a singleton and never goes away
            auto i = Platform::create_context(); 
            auto c = i->create_context(); 
        
            s = c->compile("4;");
        } // i and c variables go out of scope but are not destroyed since s still needs them
    
        s->run(); // this is fine, i and c still exist and are not leaked
    } 
    // s goes out of scope and is destroyed, followed immediately by c and then i
    
This behavior means you never have to worry about having your v8toolkit objects being cleaned up too soon and leaving your compiled scripts unrunnable or
making sure you manually clean up the v8toolkit objects in order to not have a memory leak.  

#### Running JavaScript in a separate thread

This section is intentionally after the v8toolkit Object Lifetime section because it's important to understand how the v8toolkit objects maintain their
dependencies.  The description of the dependency mechanism is expanded on in this section, so if you didn't read v8toolkit Object Lifetime (or it didn't 
make sense the first time), go back and read it (again) before continuing with this section.

In all our examples so far, the JavaScript has been executed on the current thread: context->run() or script->run() is called, the JavaScript executes immediately
and when it's done, execution resumes in your code.   However, it's quite easy to run javascript in a separate thread.  There are 3 options provided for running background
Javascript, std::async/std::future, std::thread (joinable), and std::thread(detached) and each of maintains a shared_ptr to the Script they are run from
so nothing you do while it is running can cause the objects it depends on to be destroyed.  Note that even if you don't explicitly create a Script 
and simply call context->run(some_code), a Script is created behind the scenes for you.



##### run_threaded()

Calling run_threaded() creates a new thread running the requested JavaScript and returns the std::thread object associated with that thread to the caller.  
This allows the caller to do some work and then call thread.join() to wait for the thread to complete if it hasn't already.  

The std::thread library is pretty bare compared to native threading libraries in modern OSs, but std::thread gives you access to the underlying 
implementation via thread.native_handle(), so you can use all the advanced pthreads or Windows threads functionality you want to control your
thread.

Remember, if the std::thread object returned by run_threaded() goes out of scope before you call detach() or join() on it, your program will exit 
immediately.   This is part of the C++ standard, so be careful.


##### run_detached()

Calling run_detached() is completely fire and forget.   It kicks off your javascript on a new thread and returns.   You don't get
anything back.   You cannot  find out if the thread has finished.  The code you're executing could potentially set variables to note its progress or completion, but run_detached() 
doesn't provide anything for you directly.  The detached thread will hold open the v8toolkit objects it needs until it completes.   

The implementation of run_detached literally calls run_threaded(), then thread.detach(); and returns void to the caller. 




##### run_async()

Running javascript in a std::async behaves exactly like running any other code in a std::async.   The work is put in the async, and
a std::future object is returned.   At any later time, the execution path calls future.get() and the result of the completed
async is returned.  If the async has not completed, get() will block until it has completed.  Note, future.get() does not directly return the result
of your JavaScript, it returns a `std::pair\<v8::Global<v8::Value>, ScriptPtr>`.  This means that even if you lost all your handles
to your isolate, context, and script, you can still use the results in the future, since the shared_ptr in the future is keeping everything alive
for you.

    auto script = context->compile("4");
    {
        auto future = script->run_async();
        ... do some other work here...
        auto result = future.get(); // if the async hasn't completed, this call blocks until the async completes
        // do something with result->first (if you aren't going to use the result, you should use run_threaded instead of async)
    } 
    // the future goes out of scope, so it's no longer keeping your Script so if you use the result after here
    //   you have to be careful that it's still valid (ensure neither the associated context or isolate have been destroyed)


It's important to note that only one context per isolate can be executing on a thread at a time.   This means if you spawn off a thread to run in the 
background, and then try to do something in another context in the same isolate, one of the two will block (whichever doesn't get to the internal V8 
lock in the v8::Isolate object first) until the other completes.  This means it is possible to create a deadlock if your background thread is caught 
in an infinite loop and your main thread then tries to do something requiring the lock.   


#### Writing more complicated C++ functions using the V8 API

Earlier, in the section about using STL containers, a function taking 3 ints and returning a vector with those values in it was shown.   While a function with any
number of arguments could have been made, it had to be a fixed number.   Here, we will learn how to write a function in C++ that can take any number of parameters
from javascript and decide how to act on them.

This will require delving a little bit into the V8 API however.   The API is complex, not particularly straightforward, and the documentation is hit or miss.   

The most up-to-date documentation lives here:  http://v8.paulfryzel.com/docs/master/

With the power to write more flexible functions comes the responsibility of doing this work manually.

First, a function wanting to handle the javascript parameter directly must have the following signature: a single input paramter of type 
`const v8::FunctionCallbackInfo<v8::Value> &` and a `void` return type. Only for functions with exactly this signature will the raw data will be passed through 
to the function with no "magic" behind the scenes.

Second, the function must manipulate its `FunctionCallbackInfo<T>` object which represents the javascript function invocation.  There three important parts of 
`FunctionCallbackInfo<T>`` for this are:

`Length()` - the number of parameters passed in from JavaScript.
`operator[N]` - returns the javascript parameter at position N (N must be \< Length())
`GetReturnValue()` - Used for returning a v8::Value back to the JavaScript caller

Lastly, the types must be marshalled from their JavaScript types to traditional C++ types and then back to a JavaScrit value for the return value.  There are two 
functors for doing these conversions - they are called `CastToNative<T>` and `CastToJS<T>`.  T always refers to the native
type as the JavaScript type is always v8::Value.


With this information, here is an example of a function taking an arbitrary number of parameters and returning a `std::vector<int>` with all the parameters in it:

    void make_vector(const v8::FunctionCallbackInfo<v8::Value> info &) {
        std::vector<int> v;
        for(int i = 0; i < info.Length(); i++) {
            v.push_back(CastToNative<int>(info[i]));
        }
        info.GetReturnValue.Set(CastToJS<std::vector<int>>(v));
    }

That's it.   It loops through each parameter passed in from JavaScript, runs `CastToNative<int>` on each JavaScript value to coerce it into an int
(how good that coercion is depends on how integer-y the JavaScript values are), and pushes that int onto the end of the vector. Then, to return
the result back to the JavaScript caller, it takes the vector and turns it into a v8::Value appropriate for returning back to the JavaScript caller.

If the type you want to cast to or from isn't supported, more V8 API work is in store, but fortunately the next section is:

#### Extending CastToNative and CastToJS

The two functors `CastToNative<T>` and `CastToJS<T>` are responsible for converting back and forth between v8::Value (which can represent any JavaScript type)
and a specific native type.  Most of the casts are in casts.hpp.  There are a large number of specializations - all the native C++ types as well as many 
types from std::.

The casts missing from casts.hpp are the ones responsible for handling user-defined types.  These are located in v8_class_wrapper.h.   There should be 
no need to change these.  They are only selected when no exact match is found.

There should be enough examples of how to use the V8 API for wrapping types in the existig casts for other types.   For example, to wrap a new STL 
container, look at the existing casts for STL containers: std::vector is quite simple, std::map is more complex, and std::multimap is fairly complex 
as it creates an object with vectors for values.



For more examples, look at the files in the `samples` subdirectory.  




# 'Bidirectional' Types


Bidirectional types allow for 'type' creation from both JavaScript and C++.  If a JavaScript 
'derived' type hasn't implemented a function, it will fall back to the C++ implementation.

There are many limitations to these 'types', but they are also quite powerful.  

Look at bidirectional_sample.cpp for a concrete example.  Bidirectional type generation is
supported in the ClassParser clang plugin.  