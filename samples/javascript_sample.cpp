#include <stdio.h>

#include "javascript.h"

using namespace v8toolkit;

class Foo {

};


Foo get_foo(){printf("in get_foo\n");return Foo();}

int x = 1;
int y = 2;
int y2 = 3;

auto run_tests()
{
    
    auto ih1 = PlatformHelper::create_isolate();
    auto ih2 = PlatformHelper::create_isolate();
    
    return (*ih1)([&](){
        ih1->add_print();
        
        // expose global variable x as "x" in all contexts created from this isolate
        ih1->expose_variable("x", x);
        ih1->add_function("return_hi", [](){return "hi";});
        
        auto c = ih1->create_context();
        auto c2 = ih1->create_context();
        
        c->run("5");
        c->run("println(\"x is: \", x);");
        c->run("println('return_hi():',return_hi());");
        
        ih1->expose_variable("y", y);
        ih1->add_function("return_bye", [](){return "bye";});
        
        try {
            c->run("println(y)");
        } catch(v8toolkit::ContextHelper::ExecutionError & e) {
            printf("Expected failure, as 'y' is not present in contexts created before y was added to isolate: %s\n", e.what());
        }
        try {
            c->run("println(return_bye())");
        } catch(v8toolkit::ContextHelper::ExecutionError & e) {
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
        } catch(v8toolkit::ContextHelper::ExecutionError & e) {
            printf("Expected failure, as 'y' is not present in contexts created before y was added to isolate: %s\n", e.what());
        }
        try {
            c2->run("println(return_bye())");
        } catch(v8toolkit::ContextHelper::ExecutionError & e) {
            printf("Expected failure, as 'return_bye' is not present in contexts created before return_bye was added to isolate: %s\n", e.what());
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


int main(int argc, char ** argv) {
    
    PlatformHelper::init(argc, argv);
    
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

