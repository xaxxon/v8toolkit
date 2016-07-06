//
//// this is supposed to be a copy of NONE
//#define SKIP __attribute__((annotate("v8toolkit_generate_bindings_none")))
//
//// this is supposed to be a copy of ALL
//#define EXPORT __attribute__((annotate("v8toolkit_generate_bindings_all")))
//
//#define NONE __attribute__((annotate("v8toolkit_generate_bindings_none")))
//#define SOME __attribute__((annotate("v8toolkit_generate_bindings_some")))
//#define EXCEPT __attribute__((annotate("v8toolkit_generate_bindings_except")))
//#define ALL __attribute__((annotate("v8toolkit_generate_bindings_all")))
//#define NOT_SPECIAL __attribute__((annotate("else")))
//
//
//
//#define V8TOOLKIT_BIDIRECTIONAL __attribute__((annotate("v8toolkit_generate_bidirectional")))

#include <functional>
#include <memory>

#include "class_parser.h"

class Uninteresting{};

class OnlyUsedInTemplate{};

class V8TOOLKIT_WRAPPED_CLASS FooParent {
public:
    FooParent();
    virtual void fooparent_purevirtual_tobeoverridden() = 0;
    virtual void fooparent_virtual_tobeoverridden();
    virtual void fooparent_virtual(char * a, int b, const volatile short & c);
    static int fooparent_static_method(const int *){return 8;}

    virtual int const_virtual_not_overwritten(int, int, int) const;

    char fooparent_char();
    int fp_i;
};

