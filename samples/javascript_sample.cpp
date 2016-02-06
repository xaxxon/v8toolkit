#include <stdio.h>

#include "javascript.h"

using namespace v8toolkit;

class Foo {

};


Foo get_foo(){printf("in get_foo\n");return Foo();}

int x = 1;
int y = 2;
int y2 = 3;

class TestException : std::exception {
    virtual const char * what() const noexcept override {return "TestException What";}
};

auto run_tests()
{
    
    auto ih1 = PlatformHelper::create_isolate();
    auto ih2 = PlatformHelper::create_isolate();
    
    return (*ih1)([&](){
        ih1->add_print();
        
        // expose global variable x as "x" in all contexts created from this isolate
        ih1->expose_variable("x", x);
        ih1->add_function("return_hi", [](){return "hi";});
        ih1->add_function("throw_test_exception", [](){throw TestException();});
        
        auto c = ih1->create_context();
        auto c2 = ih1->create_context();
        
        c->run("5");
        c->run("println(\"x is: \", x);");
        c->run("println('return_hi():',return_hi());");
        
        ih1->expose_variable("y", y);
        ih1->add_function("return_bye", [](){return "bye";});
        
        printf("*** Expecting exception\n");
        try {
            c->run("println(y)");
        } catch(v8toolkit::V8Exception & e) {
            printf("Expected failure, as 'y' is not present in contexts created before y was added to isolate: %s\n", e.what());
        }
        try {
            c->run("println(return_bye())");
        } catch(v8toolkit::V8Exception & e) {
            printf("Expected failure, as 'return_bye' is not present in contexts created before return_bye was added to isolate: %s\n", e.what());
        }
        
        auto c3 = ih1->create_context();
        printf("But they do exist on c3, as it was created after they were added to the isolate helper\n");
        c3->run("println('y is: ', y)");
        c3->run("println('return_bye(): ', return_bye())");
        
        (*c)([&](){
            // however, you can add them to an individual context
            c->expose_variable("y", y2);
            c->add_function("return_bye", [](){return "goodbye";});
        });
        
        printf("Now slightly different versions of 'y' and 'return_bye' have been added to c\n");
        c->run("println('y is:', y)");
        c->run("println('return_bye():', return_bye())");
        
        printf("but they're still not on c2, which was made at the same time as c\n");
        try {
            c2->run("println(y)");
        } catch(v8toolkit::V8Exception & e) {
            printf("Expected failure, as 'y' is not present in contexts created before y was added to isolate: %s\n", e.what());
        }
        try {
            c2->run("println(return_bye())");
        } catch(v8toolkit::V8Exception & e) {
            printf("Expected failure, as 'return_bye' is not present in contexts created before return_bye was added to isolate: %s\n", e.what());
        }
        
        try {
            c->run("throw_test_exception()");
        } catch(TestException & e) {
            printf("Caught TestException as expected");
        }
        
        // returning context to demonstrate how having a context alive keeps an IsolateHelper alive even though the
        //   direct shared_ptr to it is destroyed at the end of this function
        return c;
    });
}


auto test_lifetimes()
{
    std::shared_ptr<ScriptHelper> s;
    {
        std::shared_ptr<ContextHelper> c;
        {
            auto i = PlatformHelper::create_isolate();
            c = i->create_context();
        }
        // c is keeping i alive
        printf("Nothing should have been destroyed yet\n");
        
        (*c)([&](){
            s = c->compile("5");
        });
    }    
    // s is keeping c alive which is keeping i alive
    printf("Nothing should have been destroyed yet right before leaving test_lifetimes\n");
    
    return s->run_async();
    // The std::future returned from the async contains a shared_ptr to the context
}


struct A { int i=1; }; 
struct B { int i=2; }; 
struct C : A, B { int i=3; virtual ~C(){}};

class NotFamily {};

class NotWrapped {};

void run_type_conversion_test()
{
    auto i = PlatformHelper::create_isolate();
    (*i)([&]{
        i->wrap_class<C>().add_constructor("C", i->get_object_template());
        i->wrap_class<A>().add_constructor("A", i->get_object_template()).set_compatible_types<C>();
        i->wrap_class<B>().add_constructor("B", i->get_object_template()).set_compatible_types<C>();
        i->wrap_class<NotFamily>().add_constructor("NotFamily", i->get_object_template());
        i->add_function("a", [](A * a) {
            printf("In 'A' function, A::i = %d (1) &a=%p\n", a->i, a);
        });
        i->add_function("b", [](B * b) {
            printf("In 'B' function, B::i = %d (2) &b=%p\n", b->i, b);
        });
        i->add_function("c", [](C * c) {
            printf("In 'C' function, C::i = %d (3) &c=%p\n", c->i, c);
        });
        i->add_function("not_wrapped", [](NotWrapped * nw) {
            printf("In 'not_wrapped' function\n");
        });
        auto c = i->create_context();
        c->run("a(new A())");
        c->run("a(new C())");
        c->run("b(new B())");
        c->run("b(new C())");
        c->run("c(new C())");
        
        printf("The following 3 lines are the same c++ object but casted to each type's starting address\n");
        c->run("var cvar = new C(); a(cvar); b(cvar); c(cvar);");
        
        
        try {
            c->run("a(new NotFamily())");
            printf("(BAD) Didn't catch exception\n");
        } catch(...) {
            printf("(GOOD) Caught exception calling parent() function with incompatible wrapped object\n");
        }
        try {
            c->run("not_wrapped(new C())");
            printf("(BAD) Didn't catch exception calling not_wrapped()\n");
        } catch(...) {
            printf("(GOOD) Caught exception calling not_wrapped when its parameter type is unknown to v8classwrapper\n");
        }
    });
}


int main(int argc, char ** argv) {
    
    PlatformHelper::init(argc, argv);
    
    run_type_conversion_test();
    
    auto future = test_lifetimes();
    printf("Nothing should have been destroyed yet\n");
    {
        auto results = future.get();
        results.first.Reset();
        printf("Nothing should have been destroyed yet after getting future results\n");
    }
    printf("The script, context, and isolate helpers should have all been destroyed\n");

    auto context = run_tests();
    printf("The script, context, and isolate helpers should have all been destroyed\n");
    
    printf("after run_tests, one isolate helper was destroyed, since it made no contexts\n");
    
    printf("Program ending, so last context and the isolate that made it will now be destroyed\n");
}

