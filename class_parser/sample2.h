
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <set>

#include <EASTL/vector_set.h>

using namespace std;
#include "class_parser.h"

// simulate classes actually in v8_class_wrapper.h
namespace v8toolkit {
    template<class T>
    class V8ClassWrapper;


    class WrappedClassBase{};

    template<class T>
	class JSWrapper{};
}


class Uninteresting{};

class OnlyUsedInTemplate{};

template<class T, int i>
class TemplatedClass{};

class FooGrandParent : public v8toolkit::WrappedClassBase {
public:
    FooGrandParent(char, char, char);
    virtual void foo_grandparent_but_not_foo_virtual();
};

class V8TOOLKIT_WRAPPED_CLASS FooParent : public FooGrandParent {
public:
//    FooParent();
//    virtual void fooparent_purevirtual_tobeoverridden() = 0;
//    virtual void fooparent_virtual_tobeoverridden();
//    virtual void fooparent_virtual(char * a, int b, const volatile short & c);
//    static int fooparent_static_method(const int *){return 8;}
//
//    virtual int const_virtual_not_overwritten(int, int, int) const;
//
//    char fooparent_char();
//    int fp_i;

    virtual void foo_parent_virtual(int, int, int);
    virtual void foo_parent_pure_virtual(char, char, char, char) = 0;
    virtual void foo_grandparent_but_not_foo_virtual() override;

};




namespace v8toolkit {

    class EmptyFactoryBase {};

    class WrappedClassBase;
    
    template<class, class...>
	class V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS FlexibleParent : public WrappedClassBase {};
    


template<class...>
    class TypeList;
    
template<
    class Base,
    class Child,
    class ExternalTypeList,
    template<class, class...> class ParentType,
    class FactoryBase>
    class CppFactory;


/*
template<
    class,
    class Child,
    class ExternalTypeList,
    template<class, class...> class ParentType,
    class FactoryBase>
    
    class V8TOOLKIT_SKIP CppFactory;
*/
}


