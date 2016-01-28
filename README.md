##This library is not polished.  While it should work, you will have to do some customization for your own environment

## Note, the current version is not compatible with the production version of V8 because of a poorly designed v8::Local constructor.  It can easily be modified by changing some std::string parameters back to const char * but the v8 api should be changed instead..

# v8toolkit
Standalone tools for using V8.  These are helper functions for use when using the V8 API and will not, on their own, get you very far.

# v8_class_wrapper
Utilities for automatically wrapping c++ classes for use in javascript with the V8 Javascript engine - compatible with V8 v4.9.0.0 (i.e. the current API as of early 2016).  For usage, see sample.cpp.  This builds on top of the toolbox code to easily allow for complex C++ objects to be used in your javascript.

#javascript
(poorly named) Objects for creation and management of the v8 platform, isolates, and contexts.  Requires V8ClassWrapper.  This is the simplest way to embed V8 in your application, as it requires virtually no understanding of the underlying V8 APIs, but is also the least flexible for advanced use.

# Usage example:

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

