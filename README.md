# v8_toolbox.hpp
Standalone tools for using V8.  Does not require using V8ClassWrapper.  Right now methods are not in a namespace and pollute global.  For usage, see toolbox_sample.cpp

# v8-class-wrapper
Utilities for automatically wrapping c++ classes for use in javascript with the V8 Javascript engine - compatible with V8 v4.9.0.0 (i.e. the current API as of early 2016).  For usage, see sample.cpp

# These docs may be out of date but sample.cpp and toolbox_sample.cpp show current usage

```
class MyClass {
public: 
	MyClass(){}
	MyClass(int, int){}
	int some_method(int x){return 2*x;}
	void overloaded(int){}
	void overloaded(double){}
	int value;
};

	// any object that will every be created or returned by your c++ code must have it's type wrapped for each v8::Isolate it will be used in
	// even if you don't expose a constructor method to javascript
	auto & wrapper = V8ClassWrapper<MyClass>::get_instance(isolate);
	auto & different_wrapper = V8ClassWrapper<MyClass>::get_instance(a_different_isolate); // constructors/method/members must be added to this instance separately
	
	wrapped_point.add_constructor("MyClass", global_templ); // makes the default constructor available
	
	// make the second constructor visible but must have a different name - var obj = new MyClassIntInt(5,5);  
	wrapped_point.add_constructor<int,int>("MyClassIntInt", global_templ);
	
	wrapped_point.add_method(&Point::some_method, "some_method"); // some_method can now be called on a MyClass object from javascript
	
	// overloaded functions can be individually addressed, but they can't be the same name to javascript
	wrapped_point.add_method<int (Point::*)(char *)>(&Point::overloaded_method, "overloaded_method1");
	wrapped_point.add_method<int (Point::*)(int)>(&Point::overloaded_method, "overloaded_method2");
	
	wrapped_point.add_member(&Point::value, "value"); // the value member of MyClass can now be read from and written to from javascript
```

allows you to say in javascript:

```
var o = new MyClass();
var result = o.some_method(5);
o.value = result;
```
If the types you want to use aren't supported, just add them to casts.hpp.  It's pretty straightforward.  Any wrapped class is automatically supported as long as V8ClassWrapper\<Type\>::get_instance(the_correct_isolate); was called for the isolate you're running your code in.   

Obviously this is not a polished library and I'm not sure how much more work this will get, but it's at least something good to look at, if you're reasonably good with intermediate-level c++ templating syntax.


# Behaviors:
If a wrapped class type r-value is returned from a function, a default copy will be made and the underlying object will be deleted when the javascript object is garbage collected.
Subsequent calls to that function will return different javascript objects backed by different c++ objects.

If a wrapped class type reference or pointer value is returned from a function, the first time it will create a new javascript object for the object.   Subsequent calls will return the same javascript object (not different javascript objects wrapping the same c++ object).   This means you can customize the object in javascript outside of the c++ interactions and get it again later.

