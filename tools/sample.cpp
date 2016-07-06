#include "sample2.h"
//
//int do_something(int i){return i;}
//
//char do_something(char c){return c;}

class V8TOOLKIT_WRAPPED_CLASS AbstractClass {
public:
    AbstractClass(){}
    virtual void pure_virtual_function() = 0;
};

class HelperClass {
public:
    using Callback = std::function<int(char)>;
};

class V8TOOLKIT_WRAPPED_CLASS V8TOOLKIT_BIDIRECTIONAL_CLASS Foo : public FooParent {
    void foo_method(int*, int){}
    double a;
    Foo(int, char, short);
public:
    using Using=int;
    using Using2 = Using;
    V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR Foo(V8TOOLKIT_BIDIRECTIONAL_INTERNAL_PARAMETER short, int*);
    V8TOOLKIT_SKIP Foo(int, char*); // skip this constructor, otherwise name error
    V8TOOLKIT_CONSTRUCTOR(FooInt) Foo(int);
    V8TOOLKIT_SKIP void foo_explicitly_skipped();
    virtual void fooparent_purevirtual_tobeoverridden();
    virtual char const_virtual(int) const;
    int foo_int_method(char*, char){return 4;}
    virtual void fooparent_virtual_tobeoverridden();
    static int foo_static_method(const int *){return 8;}
    const Using2 & using_return_type_test();
    void call_helper_calback(HelperClass::Callback);

    HelperClass & do_foo_things(Foo &, HelperClass**&, volatile FooParent *&);

    float b;
    V8TOOLKIT_SKIP float c;
    std::unique_ptr<OnlyUsedInTemplate> unique_ptr_type_test;
    virtual void templated_input_parameter_test(std::pair<OnlyUsedInTemplate, OnlyUsedInTemplate>);
};


class Foo;


class V8TOOLKIT_WRAPPED_CLASS ConstructorTest {
 public:
    ~ConstructorTest();
};

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

int main() {}
