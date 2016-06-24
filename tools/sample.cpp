#include <string>
#include "sample2.h"
//
//int do_something(int i){return i;}
//
//char do_something(char c){return c;}

class  SPECIAL Foo {
    void foo_method(int*, int){}
public:
    int foo_int_method(char*, char){return 4;}
};

struct SPECIAL FooStruct {
    void foostruct_method(double, float){}
private:
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
