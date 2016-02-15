#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include "javascript.h"

using namespace v8toolkit;


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

// A/B/C should have interesting base offsets, with neither A nor B starting at the same place as C
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

void run_comparison_tests()
{
    auto i = PlatformHelper::create_isolate();
    i->add_assert();
    auto c = i->create_context();
    
    c->run("assert_contents(true, true);");
    bool got_expected_catch = false;
    try{
        c->run("assert_contents(true, false);");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    got_expected_catch = false;
    c->run("assert_contents(1,1);");
    try{
        c->run("assert_contents(1, 2);");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    
    got_expected_catch = false;
    c->run("assert_contents('asdf', 'asdf');");
    
    try{
        c->run("assert_contents('true', 'false');");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    
    got_expected_catch = false;
    c->run("assert_contents([1, 2, 3], [1, 2, 3]);");
    
    try{
        c->run("assert_contents([1, 2, 3], [3, 2, 1]);");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    got_expected_catch = false;    
    try{
        c->run("assert_contents([1, 2, 3], [1, 2, 3, 4]);");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    
    got_expected_catch = false;
    // these should be the same because order shouldn't matter
    c->run("assert_contents({'a': 4, 'b': 3}, {'b': 3, 'a': 4});");
    
    try{
        c->run("assert_contents({'a': 3, 'b': 3}, {'b': 3, 'a': 4});");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
    
    
    got_expected_catch = false;
    c->run("assert_contents({'a': [1, 2, 3], 'b': [4, 5]}, {'b': [4, 5], 'a': [1, 2, 3]});");
    
    try{
        c->run("assert_contents({'a': [1, 2, true], 'b': [4, 5]}, {'b': [4, 5, {'c':9}], 'a': [1, 2, true]});");
    } catch (...) {
        got_expected_catch = true;
    }
    if (!got_expected_catch) {
        assert(false);
    }
}


void test_casts()
{
    auto i = PlatformHelper::create_isolate();
    i->add_assert();
    auto c = i->create_context();

    (*c)([&]{
    
        try {
            auto isolate = i->get_isolate();
            auto context = c->get_context();
            printf("***** Testing STL container casts\n");

            std::vector<std::string> v{"hello", "there", "this", "is", "a", "vector"};
            c->add_variable("v", CastToJS<decltype(v)>()(isolate, v));
            c->run("assert_contents(v, ['hello', 'there', 'this', 'is', 'a', 'vector'])");

            std::list<float> l{1.5, 2.5, 3.5, 4.5};
            c->add_variable("l", CastToJS<decltype(l)>()(isolate, l));
            c->run("assert_contents(l, [1.5, 2.5, 3.5, 4.5]);");

            std::map<std::string, int> m{{"one", 1},{"two", 2},{"three", 3}};
            c->add_variable("m", CastToJS<decltype(m)>()(isolate, m));
            c->run("assert_contents(m, {'one': 1, 'two': 2, 'three': 3});");
    
            std::map<std::string, int> m2{{"four", 4},{"five", 5},{"six", 6}};
            c->add_variable("m2", CastToJS<decltype(m2)>()(isolate, m2));
            c->run("assert_contents(m2, {'four': 4, 'five': 5, 'six': 6});");
    
            std::deque<long> d{7000000000, 8000000000, 9000000000};
            add_variable(context, context->Global(), "d", CastToJS<decltype(d)>()(isolate, d));
            c->run("assert_contents(d, [7000000000, 8000000000, 9000000000]);");
    
            std::multimap<int, int> mm{{1,1},{1,2},{1,3},{2,4},{3,5},{3,6}};
            add_variable(context, context->Global(), "mm", CastToJS<decltype(mm)>()(isolate, mm));
            c->run("assert_contents(mm, {1: [1, 2, 3], 2: [4], 3: [5, 6]});");
    
            std::array<int, 3> a{{1,2,3}};
            add_variable(context, context->Global(), "a", CastToJS<decltype(a)>()(isolate, a));
            c->run("assert_contents(a, [1, 2, 3]);");
    
            std::map<std::string, std::vector<int>> composite = {{"a",{1,2,3}},{"b",{4,5,6}},{"c",{7,8,9}}};
            add_variable(context, context->Global(), "composite", CastToJS<decltype(composite)>()(isolate, composite));
            c->run("assert_contents(composite, {'a': [1, 2, 3], 'b': [4, 5, 6], 'c': [7, 8, 9]});");
            
            std::string tuple_string("Hello");
            auto tuple = make_tuple(1, 2.2, tuple_string);
            c->expose_variable("tuple", tuple);
            c->run("assert_contents(tuple, [1, 2.2, 'Hello'])");

            printf("Done testing STL container casts\n");
        } catch (std::exception & e) {
            printf("Cast tests unexpectedily failed: %s\n", e.what());
            assert(false);  
        }
    });
}


void test_asserts()
{
    auto i = PlatformHelper::create_isolate();
    i->add_assert();
    i->add_print();
    auto c = i->create_context();
    
    bool caught_expected_assertion = false;
    try {
        c->run("assert('false')");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    try {
        c->run("assert('0')");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    try {
        c->run("assert(\"''\")");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    try {
        c->run("assert('null')");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    try {
        c->run("assert('undefined')");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    try {
        c->run("assert('NaN')");
    } catch (V8AssertionException & e) {
        caught_expected_assertion = true;
    }
    assert(caught_expected_assertion);
    caught_expected_assertion = false;
    
    c->run("assert('true')");
    c->run("assert('1==1')");
    c->run("assert(\"'hello'\")");
    c->run("assert('[]')");
    c->run("assert('2')");
    c->run("assert('4.4')");
    c->run("if({}){println('{} is true');} else{println('{} is false');}");
    c->run("assert('({})')"); // the program "{}" is an empty program, not an empty object

    printf("Dont testing asserts\n");   
}



void require_directory_test()
{
    printf("In require directory test\n");
    auto i = PlatformHelper::create_isolate();
    (*i)([&](){
        i->add_require();
        i->add_print();
        add_module_list(*i, i->get_object_template());
        auto c = i->create_context();
        (*c)([&]{
            require_directory(*c, "modules");
            c->run("printobj(module_list())");
            
            require_directory(*c, "modules");
            c->run("printobj(module_list())");
            
            require_directory(*c, "modules");
            c->run("printobj(module_list())");
            
        });
    });
}
void run_custom_object_creator_test()
{
    
    auto i = PlatformHelper::create_isolate();
    i->add_function("create", [](const v8::FunctionCallbackInfo<v8::Value>& info){
        const auto isolate = info.GetIsolate();
        v8::HandleScope scope(isolate);
        v8::Local<v8::Object> prototype = info[0];
        if (!prototype->IsNull() && !prototype->IsJSReceiver()) {
            assert(false);
          // THROW_NEW_ERROR_RETURN_FAILURE(
          //     isolate, NewTypeError(MessageTemplate::kProtoObjectOrNull, prototype));
        }

        // Generate the map with the specified {prototype} based on the Object                               
        // function's initial map from the current native context.                                           
        // TODO(bmeurer): Use a dedicated cache for Object.create; think about                               
        // slack tracking for Object.create.                                                                 
        Handle<Map> map(isolate->native_context()->object_function()->initial_map(),
                        isolate);
        if (map->prototype() != *prototype) {
          map = Map::TransitionToPrototype(map, prototype, FAST_PROTOTYPE);
        }

        // Actually allocate the object.                                                                     
        Handle<JSObject> object = isolate->factory()->NewJSObjectFromMap(map);

        // Define the properties if properties was specified and is not undefined.                           
        Handle<Object> properties = args.atOrUndefined(isolate, 2);
        if (!properties->IsUndefined()) {
          RETURN_FAILURE_ON_EXCEPTION(
              isolate, JSReceiver::DefineProperties(isolate, object, properties));
        }

        return *object;
        
    })
    
}



int main(int argc, char ** argv) {
    
    PlatformHelper::init(argc, argv);

    run_custom_object_creator_test();

    //
    // run_type_conversion_test();
    //
    // auto future = test_lifetimes();
    // printf("Nothing should have been destroyed yet\n");
    // {
    //     auto results = future.get();
    //     results.first.Reset();
    //     printf("Nothing should have been destroyed yet after getting future results\n");
    // }
    // printf("The script, context, and isolate helpers should have all been destroyed\n");
    //
    // auto context = run_tests();
    // printf("The script, context, and isolate helpers should have all been destroyed\n");
    //
    // printf("after run_tests, one isolate helper was destroyed, since it made no contexts\n");
    //
    // printf("Running comparison tests\n");
    // run_comparison_tests();
    //
    // printf("Testing casts\n");
    // test_casts();
    //
    // printf("Testing asserts\n");
    // test_asserts();
    //
    // require_directory_test();
    
    printf("Program ending, so last context and the isolate that made it will now be destroyed\n");
}

