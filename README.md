
## Doxygen docs available here: http://xaxxon.github.io/v8toolkit/docs/html/index.html

## Tutorial for using this library

#### Install git

Type `git` on the command line.   If you don't have it, go here: https://git-scm.com/book/en/v2/Getting-Started-Installing-Git


#### Building V8

Building V8 is not a simple process.
in
"depot tools" is a collection of google build tools and is required for building V8: http://dev.chromium.org/developers/how-tos/install-depot-tools
`git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git` this then include it in the PATH environment variable (or just type in the whole path)

(following the instructions here: https://github.com/v8/v8/wiki/Using%20Git)

From here, the process diverges a bit based on what platform you're on:

[Build V8 for OS X](osx_v8_build.md)
[Build V8 for Windows (Visual Studio)](windows_v8_build.md)
[Build V8 for Linux](linux_v8_build.md)

(the following hasn't been moved into the above links yet)
Next, run `fetch v8`. This will put the source for V8 in a directory called v8.  (`fetch` is in the depot-tools you created in the step above)

For OS X, you need to tell V8 to build with libc++ (instead of libstdc++).  To do this, set the following environment variables.
They can either be set on the command line or put in your `~/.bash_profile`.  If you put them in your `.bash_profile`, you must either start a new
shell or `source` your .bash_rc like `. ~/.bash_rc` to get the environment variables in your current shell

    export CXX="`which clang++` -std=c++11 -stdlib=libc++"
    export CC="`which clang`"
    export CPP="`which clang` -E"
    export LINK="`which clang++` -std=c++11 -stdlib=libc++"
    export CXX_host="`which clang++`"
    export CC_host="`which clang`"
    export CPP_host="`which clang` -E"
    export LINK_host="`which clang++`"
    export GYP_DEFINES="clang=1 mac_deployment_target=10.7"

Again, the above lines are ONLY for OS X builds (as far as I understand).  Type `echo $CPP` to verify the environment variables are set.  If V8 is not built with libc++ in OS X, 
the code we write later in this guide will spew errors about `std::string` being an undefined symbol.


Start the build by going into the v8 directory and running `make native`.   This will build V8 static libraries for the platform of the computer.   Running `make all` will
attempt to build for x86, x64, as well as ARM and can lead to errors if the environment isn't set up for cross compiling and takes much longer.

(more options, such as building a shared library are here: https://github.com/v8/v8/wiki/Building%20with%20Gyp)

Once this finishes, from the v8 directory, type `cd out/native` and then run `./d8` (a javascript shell) to verify success. (ctrl-d to quit d8)

`Note: You can decrease the build time by around 50% by editing build/all.gyp and commenting out (with #'s) the lines about cctest.gyp and unittests.gyp`

##### Building in Windows (incomplete)

git clone depot tools as above. - $ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git

Put the new directory in your path - http://www.computerhope.com/issues/ch000549.htm

install python (2) from python.org - I used 2.7.11 -  https://www.python.org/downloads/

put python in your permanent path - it must be in your path environment variable for visual studio to use later or you will get errors in the output window in visual studio

start the bash included with depot_tools:  depot_tools\git-2.7.4-64_bin\bin/bash.exe

This next line shouldn't be needed and I don't know what it does, but if you don't use it and get errors in landmines.py, use it
> export DEPOT_TOOLS_WIN_TOOLCHAIN=0 

type >fetch v8

in v8/build there is all.sln file and that can be loaded into visual studio 2015 (I was using Update 2 when I wrote this).   It will tell you it's an older version and you need to convert it.  Just hit "ok" with all the options selected.   After it converts, build the d8 javascript shell by opening clicking "build", "build" (in the drop down), "d8".  

`NOTE: This will build with the 2013 toolchain by default.  This means you CANNOT link it with code compiled with the 2015 toolchain.  To change this, go to the Project menu, then `Properties`, `Configuration Properties`, `General`, and go to the `Platform Toolset` option and select `Visual Studio 2015 (v140)` or whatever version you want to use.  However, it probably isn't guaranteed to work in any other version of the toolset.

Also note this will build with the statically linked runtime.  As far as I understand, the runtimes of what you link to it must match.  I don't know if building with the dynamically linked runtime works or not.

In `v8/build/Debug` you should now have d8.exe.  If it isn't there, make sure you have python in your permanent PATH environment variable (following the directions in the URL above) and didn't just set it on the command line.   Visual Studio has to know where to find it.  


#### Building/installing Boost

v8toolkit uses `boost::format` to provide printf-style functions for console output and string generation.

##### Simple but long Boost install

With apt-get:  `sudo apt-get install libboost-all-dev`

OS X with brew: `brew install boost --c++11`

From source:

Get boost: http://www.boost.org/doc/libs/1_60_0/more/getting_started/unix-variants.html#get-boost

Install boost: 

Short version: untar, go into boost directory, `./bootstrap.sh`, `./b2` (this can take a while), `./b2 install` (this will install with the prefix /usr/local/include)

More detailed install information: http://www.boost.org/doc/libs/1_60_0/more/getting_started/unix-variants.html

NOTE: If you plan on using the boost libraries that have to be compiled (as opposed to header-only ones), you'll need to build boost with libc++.  Here is an example
on how to do that: https://gist.github.com/jimporter/10442880

##### Quick Boost "install"
`boost::format` is a header-only library, so you can just use the -I flag to point the compiler at the headers in the untar'd source.



#### Build v8toolkit

git clone v8toolkit:  git clone https://github.com/xaxxon/v8toolkit.git 

go into the v8toolkit directory

edit the Makefile so that V8_DIR=\<PATH_TO_V8_BASE_DIRECTORY> (under LINUX if compiling on Linux or in the `else` section if compiling on OS X)

To build the libray, on OS X, type `make tests`, on Linux, type `make tests LINUX=1`

The library and some sample programs should build.   To confirm everything is working, `make run` will run the sample programs.



#### Your First V8 program

Here is the simplest program you can write to execute some javascript using the v8toolkit library:

    #include "javascript.h"
    using v8toolkit;
    
    int main(int argc, char ** argv) {
        // any v8-specific command-line arguments will be stripped out
        Platform::init(argc, argv); 
        
        // creates a v8toolkit::Isolate which can manage some number of contexts
        auto isolate = Platform::create_isolate();
        
        // javascript doesn't have any way to print to stdout, so this exposes some handy functions to javascript
        isolate->add_print(); 
        
        // a context represents an actual javascript environment in which code is executed
        auto context = isolate->create_context(); 
        
        // Prints to the screen using the print helper function we added above
        context->run("println('Hello JavaScript World!');"); 
    }

Working backwards, a context is the container in which JavaScript code actually runs.   Any changes made to globally accessible objects in a context persist 
and will be seen any other JavaScript run within that same context.

An isolate contains the necessary state for any number of related context objects.  An isolate can also be customized so that all contexts created after the customization share that customization.   In the example above, if we made another context by calling `isolate->create_context()` a second time, this second context would also have access to println().  However, if a second isolate were made, contexts created from it would not have the print helpers unless add_print() were also to be called on the second isolate.  Lastly, regardless of how many contexts exist within an isolate, only one of them can be active at a time.   If you want multiple threads running JavaScript, you must have multiple isolates.

Platform manages the per-process configuration for V8 and is used to create isolates.  It has no interaction with your JavaScript execution.  Simply call the static ::init() method on it once and V8 is ready to go.

##### v8toolkit provides classes wrapping native V8 classes with the same names.  This means you cannot `use namespace` both the v8 and v8toolkit namespaces at the same time or you will get ambiguity errors.

#### Compiler options to build your program

Here are some example command lines for building an application using V8.  In it are some things that will need to be replaced with the actual location on the 
computer being used:

\<PATH_TO_V8_BASE_DIRECTORY> - the directory containing the v8 include directory.   This cannot include the "include/" directory.

\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR> - location where boost/format.hpp is located.  This is likely /usr/local/include and cannot include the "boost/" directory

\<YOUR_PROGRAM_CPP> - your source code

\<PATH_TO_V8TOOLKIT> - path where this library is installed and built and the file libv8toolkit.a exists

Successfully running the commands below will build your program as `./a.out`

On OS X (command line - unknown how/if xcode works):

    clang++ -std=c++14 -g -I./ -I\<PATH_TO_V8_BASE_DIRECTORY> -g -I\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR>  -Wall -Werror  YOUR_PROGRAM_CPP  
    -L\<PATH_TO_V8_BASE_DIRECTORY>/out/native/  -I\<PATH_TO_V8TOOLKIT> \<PATH_TO_V8TOOLKIT>/libv8toolkit.a -lv8_base -lv8_libbase -lv8_base -lv8_libplatform -lv8_nosnapshot -licudata -licuuc -licui18n  
    
On Linux (tested on Ubuntu Desktop 15.10)

    g++ -std=c++14 -g -I\<PATH_TO_V8TOOLKIT> -I\<PATH_TO_V8_BASE_DIRECTORY>  -I\<PATH_TO_DIRECTORY_WITH_BOOST_INCLUDE_DIR>  -Wall -Werror  \<YOUR_PROGRAM_CPP>
    -L\<PATH_TO_V8_BASE_DIRECTORY>/native/obj.target/tools/gyp/ -L\<PATH_TO_V8_BASE_DIRECTORY>/out/native/obj.target/third_party/icu/  -I\<PATH_TO_V8TOOLKIT>
    \<PATH_TO_V8TOOLKIT>/libv8toolkit.a -lv8_base -lv8_libbase -lv8_base -lv8_libplatform -lv8_nosnapshot -licudata -licuuc -licui18n  -lpthread -licuuc -licudata -ldl
    
Currently no information on windows builds using either MinGW or MSVC++


#### Exposing C++ functions to JavaScript

Simply running pure javascript as in the previous example just isn't interesting, though.  We want to cross the boundary between JavaScript and C++ seemlessly.  
Skipping some of the boilerplate in the previous example, here's how to call a custom C++ function from JavaScript.

    // just a normal function, nothing special about it
    int add_numbers(int x, int y) 
    { 
        return x + y; 
    }
    
    auto i = Platform::create_isolate();
    i->add_print();
    
    // adds a function named "js_add_numbers" to each context created from this isolate that calls 
    //   the C++ function add_numbers
    i->add_function("js_add_numbers", add_numbers);
    auto c = i->create_context();
    
    // prints 9
    c->run("println(js_add_numbers(4,5));");

This example introduces add_function() method which takes a name and "something to be called". This "something" can be a function, a functor 
(a class with an overloaded operator() method), a lambda (capturing or not), or anything else that std::function can hold.   The magic here is 
that this library knows what the parameter types and return type of the function you're calling are and tries its best to convert the javascript 
values passed in to match the types the function wants.  All the standard C++ types are supported and 
conversion happens using the same mechanism used when converting types in javascript.  User-defined types as well as many common STL containers are
supported as well and are described in detail below.


#### Exposing global C++ variables to JavaScript

Global variables (as well as class public static variables) can be exposed to javascript, as well.   

    int x = 0;
    i->expose_variable("x", x);
    ...
    c->run("x = 4; println(x);"); // prints 4
    printf("%d\n", x); // also prints 4

This works very similarly to the add_function method above.  Give it a name and the variable to expose and JavaScript now has access to it.


#### Using STL containers 

Many common STL containers are supported and behave in a fairly intuitive manner.   If the container acts mostly like an array, it is turned into
an array when passed back to JavaScript.   If the container acts more like an associatie array (key/value pairs), it is turned into a javascript 
object.

    std::vector<int> make_three_element_vector(int i, int j, int k) {
        return std::vector{i,j,k};
    }
    
    // name mismatch, but each describes what they do from the user's perspective
    i->add_function("make_three_element_array", make_three_element_vector); 
    ...
    c->run("a=make_three_element_array(2,3,5); println(a[1]);"); // prints 3

This example shows a C++ function returning a vector which is turned into a JavaScript array.   During that conversion, the std::vector is converted
to an array and then each element inside the vector is converted, so vectors containing any other supported type are supported.   This includes 
containers of containers of containers...  The add_function() method does not support funtions taking a variable number of arguments, but if you're 
willing to learn more about the V8 API, you can write a function taking a variable number of arguments without too much additional work and there's 
an example on this later.

Currently there is no support for passing a JavaScript array or object to a C++ function and having it turned into a STL container as an input
parameter.   This is not believed to be a technical limitation, just a missing featuer.


#### Exposing your C++ class to javascript

Now things really start to get interesting: user-defined types.   Most of the commonly-used features of a C++ class are supported, though there are some limitations, and other
concepts don't map directly, but are still accessible.

Let's first define a type to work with:

    class Person {
    private:
        std::string name;
        int age;
        
    public:
        Animal(std::string name, int age) : name(name), age(age) {}
        virtual speak() {printf("Hi.");}
        
        // people are very impressionable on what their favorite color is.
        std::string favorite_color = "purple";
        
        std::string get_name(){return name;}
        int get_age(){return age;}
    }

To make Person available in javascript, first tell the library it's a class it should know about.  Then tell it what parts of that class it should make 
available to javascript.  There's no requirement to make everything available, and unfortunately there's no introspection in C++ to look at the class
and make everything available automatically.

###NOTE: the following code is out of date, add_constructor must go AFTER adding members and methods and a call to finalize() stating that all members and methods are added.

    auto person_wrapper = i->wrap_class<Person>;
    
    // adds a constructor function in JavaScript called "Person"
    person_wrapper.add_constructor<std::string, int>("Person");
    
    // If you weren't familiar with the syntax for getting a method pointer, now you are.
    person_wrapper.add_method("speak", &Person::speak);
    
    // If you weren't familiar with the syntax for getting a data-member pointer, now you are.
    person_wrapper.add_member("favorite_color", &Person::favorite_color);
    
    // you can chain calls while wrapping your class
    person_wrapper.add_method("get_name", &Person::get_name).add_method("get_age", &Person::get_age);
    
    // It's very important to make the context AFTER wrapping the class.  Only contexts made from the same isolate
    //   after the class is wrapped will see your class.
    c = i->create_context();
    c->run("p = new Person();  p.speak(); println(p.get_name(), ' is ', p.get_age(), ' years old'); println('Favorite color: ', p.favorite_color);  ");

If there was also a default constructor for Person that made "John Smith" age 18, that could be added with an additional call to add_constructor(), 
but a different JavaScript function name would have to be used, like "PersonDefault".   This is a limitation of the C++ type system and how constructors 
must be called.   `add_constructor<>("PersonNoParams");` would be the syntax to add a default Person constructor if it were present.

The same limitation exists for overloaded plain functions and methods.  They can each be exposed to JavaScript, but they must have different names.

After wrapping your class, you can use your class just as any other primitive type.   On its own, as a paramter to a function, as a return type from
a function, or in an STL container.   

#### Introducing the Script

There's one more v8toolkit type we haven't talked about, since it's not needed for simple examples shown so far.  In a real-world project
the same code will be run multiple times and compiling it each time wouuld be a huge waste.   That's where the v8toolkit::Script type comes in.

    // returns a compiled script that can be run multiple times without re-compiling
    auto script = context->compile("println('Hello Javascript World!');");
    
    script->run();
    script->run();

That compiled script always runs in the context it was created in.   If you need to run the same code in another context, you will need to recompile it.


#### v8toolkit object lifetimes

In the following sections, v8toolkit object refers to all of v8toolkit Isolate, Context, and Script

In V8, a context is invalid if the isolate it was created in has been cleaned up.   Similarly, a compiled script isn't valid if its context is
destroyed or invalid.  To help manage these object dependencies, each v8toolkit object tracks their dependencies and makes
sure those dependencies are not destroyed until they are no longer needed.

If you've noticed in all the examples, the variable storing all the toolkit objects are always set as "auto".  The actual type
returned when creating these types is a std::shared_ptr\<> of type IsolatePtr, ContextPtr, ScriptPtr.  In addition, when a v8toolkit Context is created from a v8toolkit Isolate or a v8toolkit Script from a 
v8toolkit Context, the newly created object has a shared_ptr to it's parent object.  This ensures an Isolate will not be destroyed while a Context 
is depending on it or a Context destroyed while a Script is depending on it.  This means that even if your variable storing your Isolate 
goes out of scope, if you've created a Context from it and still have that object around, the associated Isolate will stick around.  This 
guarantees that as long as you can make the call to run some JavaScript code, the necessary state information will be available for it to succeed.   

To illustrate this:

    {
        std::shared_ptr<Script> s;
        {
            // Platform is a singleton and never goes away
            auto i = Platform::create_context(); // i reference count is 1
            auto c = i->create_context(); // c reference count is 1, i reference count is now 2
        
            s = c->compile("4;"); // s reference count is 1, c reference count is now 2, i is still 2
        } // i and c variables go out of scope, dropping the reference count to both their objects from 2 to 1
    
        s->run(); // this is fine, i and c still exist and are not leaked
    } 
    // s goes out of scope, dropping s's reference count from 1 to 0 causing it to be destroyed.   Destroying s
    // destroys it's shared_ptr to c, dropping c's reference count to 0 causing c to be destroyed.  Destroying
    // c destroys its shared_ptr to i, dropping i's reference count to 0 causing i to be destroyed  

This behavior means you never have to worry about having your v8toolkit objects being cleaned up too soon and leaving your compiled scripts unrunnable or
making sure you manually clean up the v8toolkit objects in order to not have a memory leak.   If you can possibly use a v8toolkit object again, it will be there for you
and if you can't, it is destroyed automatically.


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


#### Exceptions

##### The ignorance-is-bliss short version:

Exceptions work just like you'd expect.  Exceptions thrown in a C++ function called from JavaScript will immediately stop
JavaScript execution and bubble up to your script->run() call where you should catch and handle them.   There are also a few exception types 
thrown by the system: V8CompilationException and V8ExecutionException.   These override std::exception and provide the what() method which returns 
a const char * string description of what happened.  If you are going to be using the v8toolkit objects for all your V8 interactions, you can stop 
here and be happy.  No need to read any more of this section.

##### The long version:

The V8 JavaScript engine is not exception safe (and is compiled with -fno-exceptions).  Any exception making it into actual V8 library code will 
cause your application to immediately exit.  To add to the confusion, V8 has "exceptions" but they are not related, in any way, to C++ exceptions.

Knowing this, then, how can "the short version" be true?  Well, exceptions thrown in C++ code called from Script->run() (remember, a Script
is created behind the scenes if you call context->run()) are wrapped in a V8 
Exception before re-entering V8 code and then re-thrown once execution leaves the V8 library stack.  This re-thrown exception is the same exception 
as the one thrown inside the callbacks (using std::current_exception and std::exception_ptr to accomplish this).  Code not using v8toolkit objects, must 
deal with any thrown C++ exceptions on their own to make sure they don't reach any V8 code.  To throw a V8 exception, use v8::Isolate::ThrowException 
and to catch a V8 exception, declare a v8::TryCatch object on the stack where you want to catch V8 exceptions and you can test TryCatch::HasCaught() 
to see if it "has caught" and exception.  Also, often the return value of the V8 call in which the V8 exception was thrown will be a Maybe or a 
MaybeLocal which will contain no value.   That's a sign to check the TryCatch object for an Exception.   TryCatch::Exception() returns the actual v8::Value 
passed to v8::Isolate::ThrowException().  



#### Lifetime rules for wrapped objects
DO NOT READ THIS SECTION
In the process of JavaScript creating, using, and destroying JavaScript objects backed by C++ objects (like custom-wrapped user-defined classes),
it's not always clear when the underlying C++ object should be destroyed versus when it should remain.   Take object creation, for example: There
are many situations in which a user-class C++ object may be instantiated.   JavaScript calls the registered constructor (new Person()), a registered
C++ function returns a newly created object with C++'s new (a factory method, for instance, returning a Person * or Pereson &), a registered C++ 
function returning an r-value.  What about a user-defined class containing another user-defined object with an accessor to return that contained type.

How to determine the correct behavior is an ongoing process for this library.  The rest of this section is a bit of a brain-dump.   Usually the time
when it's known best what the right thing to do 
OK TO START READING AGAIN


For full example use that's guaranteed to be up to date, please see sample.cpp, toolbox_sample.cpp, and javascript_sample.cpp.  Also see threaded_smaple.cpp for examples of how to do multithreaded calls.


# Behaviors:
If a wrapped class type r-value is returned from a function, a default copy will be made and the underlying object will be deleted when the javascript object is garbage collected.
Subsequent calls to that function will return different javascript objects backed by different c++ objects.

If a wrapped class type reference or pointer value is returned from a function, the first time it will create a new javascript object for the object.   Subsequent calls will return the same javascript object (not different javascript objects wrapping the same c++ object).   This means you can customize the object in javascript outside of the c++ interactions and get it again later.

