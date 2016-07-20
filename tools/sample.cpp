#include "sample2.h"
//
//int do_something(int i){return i;}
//
//char do_something(char c){return c;}
//
//class V8TOOLKIT_WRAPPED_CLASS AbstractClass {
//public:
//    AbstractClass(){}
//    virtual void pure_virtual_function() = 0;
//};
//
//class HelperClass {
//public:
//    using Callback = std::function<int(char)>;
//};

template<class T>
class V8ClassWrapper;

// This puts the annotation on each instantiated type of the template, not the template itself
template<class T>
class V8TOOLKIT_WRAPPED_CLASS
MyTemplate {};

template<class T>
class V8TOOLKIT_WRAPPED_CLASS DerivedFromMyTemplate : public MyTemplate<T> {};

class V8TOOLKIT_WRAPPED_CLASS  V8TOOLKIT_BIDIRECTIONAL_CLASS
//V8TOOLKIT_IGNORE_BASE_TYPE(MyTemplate<int>)
V8TOOLKIT_USE_BASE_TYPE(FooParent)
Foo : public FooParent, public MyTemplate<int> {

//    struct NestedFooStruct{};
//
//    void foo_method(int*, int){}
//    double a;
public:
    using Using=int;
    using Using2 = Using;
    V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR Foo(int, char, short);

    MyTemplate<int> my_template_int;
    MyTemplate<char> my_template_char;

    DerivedFromMyTemplate<short> derived_my_template_short;
    DerivedFromMyTemplate<char*> derived_my_template_charp;

    template<class T2>
	const T2& templated_function(const T2 & t){return t;};
    
//    V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR Foo(V8TOOLKIT_BIDIREC TIONAL_INTERNAL_PARAMETER short, int*);
//    V8TOOLKIT_SKIP Foo(int, char*); // skip this constructor, otherwise name error
//    V8TOOLKIT_CONSTRUCTOR(FooInt) Foo(int);
//    V8TOOLKIT_SKIP void foo_explicitly_skipped();
//    virtual void fooparent_purevirtual_tobeoverridden();
//    virtual char const_virtual(int) const;
//    int foo_int_method(char*, char){return 4;}
//    virtual void fooparent_virtual_tobeoverridden();
//    static int foo_static_method(const int *){return 8;}
//    const Using2 & using_return_type_test();
//    std::string take_and_return_string(string);
//    const std::string take_and_return_const_string(const string);
//    volatile const std::string & take_and_return_const_volatile_string(const volatile string *&);
    const volatile map<const volatile int*&,const volatile Using2*&>*& map_test(const volatile std::map<const volatile Using2 *&,
            const volatile std::set<const volatile int*&>*&>*&);
//
//    void nested_foo_struct_test(const NestedFooStruct *&);
//    void call_helper_callback(HelperClass::Callback);
//
//    HelperClass & do_foo_things(Foo &, HelperClass**&, volatile FooParent *&);
//
//    float b;
//    V8TOOLKIT_SKIP float c;
//    std::unique_ptr<OnlyUsedInTemplate> unique_ptr_type_test;
//    virtual void templated_input_parameter_test(std::pair<OnlyUsedInTemplate, OnlyUsedInTemplate>);
//
//    TemplatedClass<HelperClass, 5> test_method_with_templated_types(const TemplatedClass<const Using2*&, 8828>****&);

    V8TOOLKIT_EXTEND_WRAPPER static void wrapper_extension(V8ClassWrapper<Foo> &);
};




//
//class Foo;
//
//
//class V8TOOLKIT_WRAPPED_CLASS ConstructorTest {
// public:
//    ~ConstructorTest();
//};

//struct ALL FooStruct {
//    int i;
//    void foostruct_method(double, float){}
//    static int static_method(const int *){return 8;}
//private:
//    char j;
//    char foostruct_char_method(const int &){return 'd';}
//};

//
//class NOT_SPECIAL Foo2 { };
//
//struct Bar { };
//
//// this is the only one that should match
//struct SPECIAL Baz { };
//

int main() {
    Foo f(5,5,5);
    f.templated_function(5);
    f.templated_function<short>(5);
    f.templated_function<long>(5);
        f.templated_function<unsigned int>(5);
	    
}
