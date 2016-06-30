#include "sample2.h"
//
//int do_something(int i){return i;}
//
//char do_something(char c){return c;}


class V8TOOLKIT_ALL FooParent {
public:
    FooParent();
    virtual void fooparent_purevirtual() = 0;
    virtual void fooparent_purevirtual_tobeoverridden() = 0;
    virtual void fooparent_virtual_tobeoverridden();
    virtual void fooparent_virtual(char * a, int b, const volatile short & c);
    static int fooparent_static_method(const int *){return 8;}

    virtual int const_virtual_not_overwritten(int, int, int) const;

    char fooparent_char();
    int fp_i;
};


class V8TOOLKIT_ALL V8TOOLKIT_BIDIRECTIONAL Foo : public FooParent {
    void foo_method(int*, int){}
    double a;
public:
    Foo();
    V8TOOLKIT_NONE void foo_explicitly_skipped();
    virtual void fooparent_purevirtual_tobeoverridden();
    virtual char const_virtual(int) const;
    int foo_int_method(char*, char){return 4;}
    virtual void fooparent_virtual_tobeoverridden();
    static int foo_static_method(const int *){return 8;}

    float b;
    V8TOOLKIT_NONE float c;
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
