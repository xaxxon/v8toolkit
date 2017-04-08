#include <functional>

#include "sample2.h"

template<class T>
class V8TOOLKIT_WRAPPED_CLASS NeedIncludeForTemplatedType {};


namespace v8 {
    template<class> class Local;
    class FunctionTemplate;
};


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
class HelperClass {
public:
   using Callback = std::function<int(char)>;
};




class ThisShouldNotMatch : public v8toolkit::JSWrapper<int>, public v8toolkit::WrappedClassBase {};
class WrappedClass : public v8toolkit::WrappedClassBase {
public:
    WrappedClass(int a, int b = 2, int c = 3, WrappedClass * wc_ptr = nullptr);
    double double_member_readwrite;
    V8TOOLKIT_READONLY double double_member_readonly1;
    double const double_member_readonly2;
    std::vector<std::string> vector_of_ints;
    std::vector<WrappedClass *> vector_of_selfs;

    V8TOOLKIT_EXTEND_WRAPPER static void extend_wrapper();
    V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension();

    int simple_member_function(char const * some_string = "asdf");
    static std::string simple_static_function(double some_double);

    void std_function_default_parameter(std::function<void()> some_function = std::function<void()>());

    enum class EnumClass{A, B, C, D};

    template<class A = int, int b = 4>
    void templated_method();

    void after_templated_method();

};

class WrappedClassDerived : public WrappedClass {
public:
    double double_member_readwrite2;
WrappedClassDerived();
    static void static_method_with_no_constructor_on_class(int, int=5, char const * = "five");
    void method_with_no_constructor_on_class(int, int const & =5, char const * = "five", WrappedClassDerived const & s = {});
};

class V8TOOLKIT_SKIP DoNotWrapEvenThoughInheritsFromWrapped;
class DoNotWrapEvenThoughInheritsFromWrapped : public WrappedClass {};


namespace Test {
    template<class = char> class V8TOOLKIT_WRAPPED_CLASS MyTemplate;
    template<class T> class MyTemplate<vector<T>> {};
}



using asdf = int;

namespace v8toolkit {

    template<
	class Base,
	class Child,
	class ExternalTypeList = TypeList<>,
	template<class, class...> class ParentType = FlexibleParent,
	class FactoryBase = EmptyFactoryBase>
    class CppFactory;

    
    template<class Base, class Child, class... ExternalConstructorParams, template<class, class...> class ParentType, class FactoryBase>
    class CppFactory<Base, Child, TypeList<ExternalConstructorParams...>, ParentType, FactoryBase> :
	    public ParentType<Base, TypeList<ExternalConstructorParams...>, FactoryBase> {
    };



}




template<class T>
class  DerivedFromWrappedClassBase : public Test::MyTemplate<vector<int>>, public v8toolkit::WrappedClassBase {
public:
    void function_in_templated_class(T t){

    }
};


class UnwrappedClassThatIsUsed {};


class V8TOOLKIT_WRAPPED_CLASS  V8TOOLKIT_BIDIRECTIONAL_CLASS
//V8TOOLKIT_IGNORE_BASE_TYPE(MyTemplate<int>)
V8TOOLKIT_USE_BASE_TYPE(FooParent)
Foo : public FooParent, public Test::MyTemplate<vector<int>> {
    struct NestedFooStruct{};


    void foo_method(int*, int){}


    double a;
public:
    using Using=int;
    using Using2 = Using;
    V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR Foo(int, char, short &&);

    UnwrappedClassThatIsUsed uctiu;
    
    Test::MyTemplate<vector<int>> my_template_int;
    Test::MyTemplate<vector<char>> my_template_char;

    DerivedFromWrappedClassBase<short> derived_my_template_short;
    DerivedFromWrappedClassBase<char*> derived_my_template_charp;

    std::vector<int> returns_vector_of_ints();
    std::map<string, int> returns_map_of_string_to_int();


    const asdf const_typedef_to_int = 1;


    template<class T2>
	const T2& templated_function(const T2 & t){return t;};


   V8TOOLKIT_SKIP Foo(int, char*); // skip this constructor, otherwise name error

   V8TOOLKIT_USE_NAME(FooInt) Foo(int); // This constructor gets different JS name - FooInt
   V8TOOLKIT_SKIP void foo_explicitly_skipped();
   virtual void fooparent_purevirtual_tobeoverridden();
   virtual char const_virtual(int) const;


    /**
     * description
     * @param a something -- missing a named parameter shouldn't cause the plugin to crash
     */
    void broken_comment(int);

    /**
     * Test comment for foo_int_method
     * @param a some string
     * @param b some character
     * @return some made up number
     */
   int foo_int_method(char* a, char b = 'a'){return 4;}
   virtual void fooparent_virtual_tobeoverridden();
   static int foo_static_method(const int *){return 8;}
   const Using2 & using_return_type_test();
   std::string take_and_return_string(string);
   const std::string take_and_return_const_string(const string);
   volatile const std::string & take_and_return_const_volatile_string(const volatile string *&);
    const volatile map<const volatile int*&,const volatile Using2*&>*& map_test(const volatile std::map<const volatile Using2 *&,
            const volatile std::set<const volatile int*&>*&>*&);

   void nested_foo_struct_test(const NestedFooStruct *&);
   void call_helper_callback(HelperClass::Callback);

   HelperClass & do_foo_things(Foo & foo, HelperClass**&, volatile FooParent *&);

   float b;
   V8TOOLKIT_SKIP float c;
   std::unique_ptr<OnlyUsedInTemplate> unique_ptr_type_test;
   virtual void templated_input_parameter_test(std::pair<OnlyUsedInTemplate, OnlyUsedInTemplate>);

   TemplatedClass<HelperClass, 5> test_method_with_templated_types(const TemplatedClass<const Using2*&, 8828>****&);

    V8TOOLKIT_EXTEND_WRAPPER
    static void wrapper_extension(v8toolkit::V8ClassWrapper<Foo> &);

    V8TOOLKIT_CUSTOM_EXTENSION
    static void custom_extension(v8::Local<v8::FunctionTemplate> & function_template);


    void same_name(int);

    V8TOOLKIT_USE_NAME(same_name_2)
    void same_name(char*);


    // just iterating through methods got foo_parent_virtual, foo_parent_pure_virtual
    virtual void foo_parent_virtual(int, int, int) override;
    virtual void foo_parent_pure_virtual(char, char, char, char) override;

    operator int();

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

    using MyType V8TOOLKIT_NAME_ALIAS V8TOOLKIT_WRAPPED_CLASS = DerivedFromWrappedClassBase<int>;


int main() {

    v8toolkit::CppFactory<int, char, v8toolkit::TypeList<double>> factory;
    
    
    Foo f(5,5,5);

    f.templated_function(5);
    f.templated_function<short>(5);
    f.templated_function<long>(5);
        f.templated_function<unsigned int>(5);
	DerivedFromWrappedClassBase<int> dfwcb;
	dfwcb.function_in_templated_class(5);


	NeedIncludeForTemplatedType<function<bool(Uninteresting&)>> uninteresting;
    //    DerivedFromWrappedClassBase<char>;
}

