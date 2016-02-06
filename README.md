##This library is not polished.  While it should work, you will have to do some customization for your own environment

## This library requires a version of V8 gotten from source after 1/30/16 or version 4.11.0.0 or later

## Note, the current version is not compatible with the production version of V8 because of a poorly designed v8::Local constructor.  It can easily be modified by changing some std::string parameters back to const char * but the v8 api should be changed instead..

## Doxygen docs available here: http://xaxxon.github.io/v8-class-wrapper/docs/html/index.html

# v8toolkit
Standalone tools for using V8.  These are helper functions for use when using the V8 API and will not, on their own, get you very far.  You must have a working knowledge of V8, but
the tools in here will help you with a lot of common tasks that are time-consuming to write out on their own.

# v8_class_wrapper
Utilities for automatically wrapping c++ classes for use in javascript with the V8 Javascript engine - compatible with V8 v4.9.0.0 (i.e. the current API as of early 2016).  For usage, see sample.cpp.  This builds on top of the toolbox code to easily allow for complex C++ objects to be used in your javascript.

#javascript
(poorly named) A system for creation and management of the v8 platform, isolates, and contexts.  Requires V8ClassWrapper.  This is the simplest way to embed V8 in your application, as it requires virtually no understanding of the underlying V8 APIs.

# Usage example:

#### Your First V8 program

Here is the simplest program you can write to execute some javascript:

    #include "javascript.h"
    using v8toolkit;
    
    int main(int argc, char ** argv) {
        // any v8-specific command-line arguments will be stripped out
        PlatformHelper::init(argc, argv); 
        
        // creates an isolate helper which can manage some number of contexts
        auto isolate_helper = PlatformHelper::create_isolate();
        
        // javascript doesn't have any way to print to stdout, so this exposes some handy functions to javascript
        isolate_helper->add_print(); 
        
        // a context represents an actual javascript environment in which code is executed
        auto context_helper = isolate_helper->create_context(); 
        
        // Prints to the screen using the print helper function we added above
        context_helper->run("println('Hello JavaScript World!');"); 
    }

Working backwards, a context is the container in which JavaScript code actually runs.   Any changes made to globally accessible objects remains in the context
and will be seen any other JavaScript run within that same context.   

An isolate contains any number of relatex context objects.   Objects in a context can be moved to another context in the same isolate.   An isolate can be customized
so that all contexts created after the customization share that customization.   In the example above, if we made another context by callig isolate_helper->create_context(), 
it would also have access to the print helpers added by add_print().    However if another isolate were made, contexts created from it would not have the print helpers unless
add_print() was also called on that second isolate.  Also, only one thread can be in any context in a given isolate.   If you want multiple threads running JavaScript, you
must have multiple isolates (which prevents directly moving javascripts between them since they are different isolates).

PlatformHelper manages the per-process configuration for V8 and creating isolates.  It has no interaction with your JavaScript execution. 

#### Exposing C++ functions to javascript

Simply running pure javascript isn't interesting, though.  We want to cross the boundary between JavaScript and C++ seemlessly.  Skipping the boilerplate in the first example,
here's how to call a custom C++ function from JavaScript.  (i = isolate_helper, c = context_helper)

    // just a normal function, nothing special about it
    int add_numbers(int i, int j) 
    { 
        return i + j; 
    }
    
    auto i = PlatformHelper::create_isolate();
    i->add_print();
    
    // adds a function named "js_add_numbers" to each context created from this isolate that calls 
    //   the C++ function add_numbers
    i->add_function("js_add_numbers", add_numbers);
    auto c = i->create_context();
    
    // prints 9
    c->run("println(js_add_numbers(4,5));");

This introduces add_function() which takes a name to expose the function as in JavaScript and "something to be called" when the javascript function
is invoked.  This can be a function, a functor (a class with an overloaded operator() method), a lambda (capturing or not), or anything else that
std::function can hold.   The magic here is that this library knows what the parameter types and return type of the function you're calling are and
tries its best to convert the javascript values passed in to match the types the function wants.  All the standard C++ types are supported and 
conversion happens using the same mechanism used when converting types in javascript.  User-defined types as well as many common STL containers are
supported as well, but we'll get to that later.

#### Exposing global C++ variables to JavaScript

Global variables (as well as class public statics) can be exposed to javascript, as well.   

    int x = 0;
    i->expose_variable("x", x);
    ...
    c->run("x = 4; println(x);"); // prints 4
    printf("%d\n", x); // also prints 4

#### Using STL containers 

Many common STL containers are supported and behave in a fairly predictable manner.   If the container acts mostly like an array, it is turned into
an array when passed back to JavaScript.   If the container acts more like an associatie array (key/value pairs), it is turned into a javascript 
object.

    std::vector<int> make_three_element_vector(int i, int j, int k) {
        return std::vector{i,j,k};
    }
    
    // name mismatch, but each describes what they do from the user's perspective
    i->add_function("make_three_element_array", make_three_element_vector); 
    ...
    c->run("a=make_three_element_array(2,3,5); println(a[1]);"); // prints 3

This example shows a C++ function returning a vector and it being turned into a JavaScript array.   During that conversion, the std::vector is turned
into an array and then each element inside the vector is converted, so vectors containing any other supported type are supported.   This includes 
maps of std::string's to vectors of ints (which would be returned as a JavaScript object value where the keys are strings and the values are arrays of 
ints).  This method of exposing functions to javascript does not support variable arguments, but, if you're willing to learn more about the V8 API,
you can write a function taking a variable number of arguments without too much additional work and there's an example of this later.

Currently there is no support for passing a JavaScript array or object to a C++ function and having it turned into a STL container as an input
parameter.   This is not believed to be a technical limitation, just a missing featuer.

#### Exposing your C++ class to javascript

Now things really start to get interesting: user-defined types.   Most features of a class are supported, though there are some limitations, and some
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

To make Person available in javascript, first tell the library it's a class it should know about, then tell it what parts of that class it should make 
available to javascript.  There's no requirement to make everything available and unfortunately there's no introspection in C++ to look at the class
and make everything available automatically.

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

If you class has multiple constructors, for example, if there were a default constructor for person that made "John Smith" age 18, that could also
be added with an additional call to add_constructor(), but a different JavaScript function name would have to be used, like "PersonNoParams" or 
something.   This is a limitation of the C++ type system and how constructors must be called (if you know this or any other specified limitation
to be false, please let me know).   add_constructor<>("PersonNoParams"); would be the syntax if Person had a default constructor.

The same limitation exists for overloaded plain functions and methods.  They can each be exposed to JavaScript, but they must have different names.

After wrapping your class, you can use your class just as any other primitive type.   On its own, as a paramter to a function, as a return type from
a function, or in an STL container.   

#### Introducing the ScriptHelper

There's one more "helper" type we haven't talked about, since it's not needed for simple examples, but it's likely the same code will be run multiple
times and compiling it each time is a huge waste.   That's where the ScriptHelper type comes in.

    // returns a compiled script that can be run multiple times without re-compiling
    auto script = context->compile("println('Hello Javascript World!');");
    
    script->run();
    script->run();

That compiled script always runs in the context it was created in.   If you need to run the same code in another context, you will need to recompile it.


#### *Helper object lifetimes

If you're noticing that all the examples use the c++11 "auto" type for holding the helpers, the reason is that they're always returned as 
std::shared_ptr<[HelperType]>.  If you're not familiar with shared_ptr, it holds a reference to an object allocated with new and when the last
shared_ptr() goes away, delete is called on the object it points to.   Not only is a shared pointer returned to the user when a Helper is created, but
another one is stored when you create a *Helper object from another *Helper.   This means that even if your variable storing your IsolateHelper goes 
out of scope, if you've created a ContextHelper from it and still have that object around, the associated IsolateHelper will stick around.   This is
very important, since the v8::Isolate stored in the IsolateHelper is required for your context to function.   The same is true for ScriptHelper objects
keeping their ContextHelper object around (which in turn keeps its IsolateHelper around).   This guarantees that as long as you can make the call to
run some JavaScript code, the necessary state information will be available for it to succeed.   To illustrate this:

    {
        std::shared_ptr<ScriptHelper> s;
        {
            auto i = PlatformHelper::create_context(); // PlatformHelper is a singleton and never goes away
            auto c = i->create_context();
        
            // remember, S is declared in an outer scope
            s = c->compile("4;"); // 4 is valid javascript
        } // i and c are not destroyed even though they can no longer be referenced, because s is still available
    
        s->run(); // this is fine, i and c still exist and are not leaked
    } 
    // s has gone out of scope and is destroyed.   After it cleans up, it's shared_ptr to c (which is the shared_ptr to c)
    //   is cleaned up causing c's destructor to be called.   After c's destructor has run, it's shared_ptr to i
    //   (again, the last shared_ptr to i) is cleaned up causing i's destructor to be called.

This behavior means you never have to worry about having your *Helper objects being cleaned up too soon and leaving your compiled scripts unrunnable or
making sure you manually clean up the *Helpers in order to not have a memory leak.   If you can possibly use a *Helper again, it will be there for you
and if you can't, it is destroyed automatically.  Also, if want to get a handle from a ScriptHelper to its ContextHelper or a ContextHelper to its
IsolateHelper, there are methods on those classes for that.

#### Running JavaScript in a separate thread

in all our examples so far, the JavaScript has been executed on the current thread: ->run() is called, the JavaScript executes and when it's done,
execution resumes in your code.   However, it's quite easy to run javascript in a separate thread.  There are 3 options for how to run background
Javascript, std::async/std::future, std::thread (joinable), and std::thread(detached).  This section is after the Helper Lifetime section because it's
important to understand that the methods for running JavaScript in a separate thread all maintain a shared_ptr to the ScriptHelper they were run from
(which in turn keeps the associated ContextHelper and IsolateHelpers around).  Also, even if you don't explicitly create a ScriptHelper and call
context->run(some_javascript_source), a ScriptHelper object is implicitly created behind the scenes and is destroyed automatically when the execution
is complete.

If you've shared existing C++ variables or objects in multiple isolates, it is your job to make sure there are no race conditions if you run multiple
contexts (from different isolates) at the same time.

##### run_detached()

Calling run_detached() is completely fire and forget.   It creates a thread for your code to run on, kicks it off, and returns.   You don't get
anything back.   The detached thread will hold open the *Helpers it needs until it completes, but that is all invisible to you.   You cannot find
when or even if the thread will end.  The code you're executing could set variables to note its progress or completion, but run_async() doesn't 
provide anything on its own.

##### run_threaded()

Using run_threaded() does the same as run_detached, but returns the std::thread object associated with the thread.  This allows the user to call
thread.join() to wait for the thread to complete and know that it has finished.   Calling thread.detach() is exactly the same as what run_detached()
does, so if the thread won't be joined, just call run_detached() instead.  The implementation of run_detached literally calls run_threaded(), then
thread.detach(); and returns void to the caller. 

The std::thread library is pretty bare compared to native threading libraries in modern OSs, but std::thread gives you access to the underlying 
implementation via thread.native_handle(), so you can use all the advanced pthreads or Windows threads functionality you want to control your
thread.

Remember, if the std::thread object returned by run_threaded goes out of scope before detach() or join() are called, your program will exit 
immediately.   This is part of the C++ standard, so be careful.

##### run_async()

Running javascript in a std::async behaves exactly like running any other code in a std::async.   The work is put in the async, and
a std::future object is returned.   Later, after doing some other unrelated work, the main thread calls future.get() and the result of the completed
async is returned.  If the async has not completed, get() will block until it has completed.  Note, future.get() does not directly return the result
of your JavaScript, it returns a std::pair<v8::Global<v8::Value>, std::shared_ptr<ScriptHelper>>.  This means that even if you lost all your handles
to your isolate, context, and script, you can still use the results in the future, since the shared_ptr in the future is keeping everything alive
for you.

    auto script = context->compile("4");
    {
        auto future = script->run_async();
        ... do some other work here...
        auto result = future.get(); // if the async hasn't completed, this call blocks until the async completes
        // do something with result->first (if you aren't going to use the result, you should use run_threaded instead of async)
    } 
    // the future goes out of scope, so it's no longer keeping your ScriptHelper so if you use the result after here
    //   you have to be careful that it's still valid (neither the associated context or isolate have been destroyed)


Remember, only one context per isolate can be
executing on a thread at a time.   This means if you spawn off a thread to run in the background, and then try to do something in another context
in the same isolate, one of the two will block (whichever doesn't get to the internal V8 lock in the v8::Isolate object first) until the other completes.
This means it is possible to create a deadlock if your background thread is caught in an infinite loop and your main thread then tries to do something
requiring the lock.   


#### Writing more complicated C++ functions using the V8 API
[Variable-argument functions by using V8 callback info object directly - are return types automatically grabbed or do you have to set it manually in the info object?]

#### Lifetime rules for wrapped objects
[wrapped-object lifetime - destructorbehavior stuff goes here]

The V8 JavaScript engine is not thread safe, but exceptions thrown in your C++ called from javascript are wrapped and re-thrown as the same exception
so you should wrap calls to ->run() with a try/catch.   Even if your code doesn't throw exceptions, V8 can throw it's own "exceptions" (V8 exceptions 
don't rely on the C++ exception mechanism) if you try to access an attribute on an undefined value.   These exceptions are of type 
V8Exception (inheriting from std::exception) and provide the expected what() method which returns a string describing the exception.  If this isn't 
sufficient, the actual JavaScript value thrown by V8 can be obtained, but using that requires directly using the V8 API.  Usually the string is
sufficient as it will say something like "Reference error: a is not defined" if you try to access a variable that doesn't exist.  There are also 
compiler errors thrown as CompilationError (again, with a what() method returning a descriptive string).  This allows you to tell if your call to
->run() with source code (versus a compiled script) failed during compilation or execution.   



	#include "javascript.h"
	using v8toolkit;

	class MyClass{
	public:
	    MyClass(int x) : x(x){}
		int x;
		int add_to_x(int y){return x + y;}
	};

	int y = 12;

	int main(int argc, char ** argv) {

		PlatformHelper::init(argc, argv);
		
		auto isolate_helper = PlatformHelper::create_isolate();
		isolate_helper.expose_variable("y", y); // exposes the global variable y as "y" within javascript
	
		auto class_wrapper = isolate_helper.wrap_class<MyClass>();
		class_wrapper.add_constructor<int>("MyClass");
		class_wrapper.add_member("x", &MyClass::x); // make MyClass::x directly accessible within javascript
		class_wrapper.add_method("add_to_x", &MyClass::add_to_x);
		
		auto context_helper = isolate_helper->create_context(); // all code must be run in a context
		context_helper.run("var myclass = new MyClass(5); myclass.add_to_x(y); myclass.x = 3; myclass.add_to_x(5);");
	}

For full example use that's guaranteed to be up to date, please see sample.cpp, toolbox_sample.cpp, and javascript_sample.cpp.  Also see threaded_smaple.cpp for examples of how to do multithreaded calls.


# Behaviors:
If a wrapped class type r-value is returned from a function, a default copy will be made and the underlying object will be deleted when the javascript object is garbage collected.
Subsequent calls to that function will return different javascript objects backed by different c++ objects.

If a wrapped class type reference or pointer value is returned from a function, the first time it will create a new javascript object for the object.   Subsequent calls will return the same javascript object (not different javascript objects wrapping the same c++ object).   This means you can customize the object in javascript outside of the c++ interactions and get it again later.

