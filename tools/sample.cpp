#include <string>
#include "sample2.h"
//
//int do_something(int i){return i;}
//
//char do_something(char c){return c;}

class  SPECIAL Foo {
    void foo_method(int*, int){}
    double a;
public:
    int foo_int_method(char*, char){return 4;}
    float b;
};

struct SPECIAL FooStruct {
    int i;
    void foostruct_method(double, float){}
    static int static_method(const int *){return 8;}
private:
    char j;
    char foostruct_char_method(const std::string &){return 'd';}
};

//
//class NOT_SPECIAL Foo2 { };
//
//struct Bar { };
//
//// this is the only one that should match
//struct SPECIAL Baz { };
//

int main() {}
