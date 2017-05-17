#include <v8_class_wrapper.h>
#include <javascript.h>
#include "testing.h"

using namespace v8toolkit;

class TypeCheckerTest_ParentClass : public v8toolkit::WrappedClassBase {};
class TypeCheckerTest_ChildClass : public TypeCheckerTest_ParentClass {};


class TypeCheckerFixture : public PlatformFixture {

public:
    IsolatePtr i;
    ContextPtr c;
    TypeCheckerFixture() {
        i = v8toolkit::Platform::create_isolate();
        i->add_assert();
    }

    void create_context() {
        c = i->create_context();

        c->add_function("EXPECT_TRUE", [](bool expected_true_value) {
            EXPECT_TRUE(expected_true_value);
        });
    }

};

TEST_F(TypeCheckerFixture, NumberTypes) {
    this->create_context();

    (*c)([&]{
        EXPECT_EQ(CastToNative<bool>()(*i, CastToJS<bool>()(*i, true)), true);
        EXPECT_EQ(CastToNative<bool>()(*i, CastToJS<bool>()(*i, false)), false);
        EXPECT_EQ(CastToNative<char>()(*i, CastToJS<char>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<wchar_t>()(*i, CastToJS<wchar_t>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<char16_t>()(*i, CastToJS<char16_t>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<char32_t>()(*i, CastToJS<char32_t>()(*i, 5)), 5);

        EXPECT_EQ(CastToNative<unsigned char>()(*i, CastToJS<unsigned char>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<short >()(*i, CastToJS<short>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<unsigned short>()(*i, CastToJS<unsigned short>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<int>()(*i, CastToJS<int>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<unsigned int>()(*i, CastToJS<unsigned int>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<long>()(*i, CastToJS<long>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<unsigned long>()(*i, CastToJS<unsigned long>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<long long>()(*i, CastToJS<long long>()(*i, 5)), 5);
        EXPECT_EQ(CastToNative<unsigned long long>()(*i, CastToJS<unsigned long long>()(*i, 5)), 5);

        EXPECT_EQ(CastToNative<float>()(*i, CastToJS<float>()(*i, 5.5)), 5.5);
        EXPECT_EQ(CastToNative<double>()(*i, CastToJS<double>()(*i, 5.5)), 5.5);
        EXPECT_EQ(CastToNative<long double>()(*i, CastToJS<long double>()(*i, 5.5)), 5.5);
    });
}

TEST_F(TypeCheckerFixture, StringTypes) {
    this->create_context();

    (*c)([&] {

        EXPECT_EQ(CastToNative<std::string>()(*i, CastToJS<std::string>()(*i, "test string")), "test string");

        // These return unique_ptr<char[]> because otherwise there wouldn't be any memory for the char * to point tos
        EXPECT_STREQ(CastToNative<char *>()(*i, CastToJS<char const *>()(*i, "test string")).get(), "test string");
        EXPECT_STREQ(CastToNative<char const *>()(*i, CastToJS<char const *>()(*i, "test string")).get(), "test string");
    });
}


TEST_F(TypeCheckerFixture, Vectors) {

    this->create_context();

    (*c)([&] {

        std::vector<std::string> v{"hello", "there", "this", "is", "a", "vector"};
        c->add_variable("v", CastToJS<decltype(v)>()(*i, v));
        c->run("assert_contents(v, ['hello', 'there', 'this', 'is', 'a', 'vector'])");

        std::vector<std::string> const cv{"hello", "there", "this", "is", "a", "vector"};
        c->add_variable("cv", CastToJS<decltype(cv)>()(*i, cv));
        c->run("assert_contents(cv, ['hello', 'there', 'this', 'is', 'a', 'vector'])");


        // vector
        {
            auto js_vector = c->run("[1, 2, 3]");
            auto vector = CastToNative<std::vector<int>>()(*i, js_vector.Get(*i));
            assert(vector.size() == 3);
            assert(vector[2] == 3);
        }
        // const vector
        {
            auto js_vector = c->run("[1, 2, 3]");
            auto vector = CastToNative<std::vector<int> const>()(*i, js_vector.Get(*i));
            assert(vector.size() == 3);
            assert(vector[2] == 3);
        }

    });
}


TEST_F(TypeCheckerFixture, ContainerTypes) {

    this->create_context();

    (*c)([&] {



        std::list<float> l{1.5, 2.5, 3.5, 4.5};
        c->add_variable("l", CastToJS<decltype(l)>()(*i, l));
        c->run("assert_contents(l, [1.5, 2.5, 3.5, 4.5]);");

        std::map<std::string, int> m{{"one", 1},{"two", 2},{"three", 3}};
        c->add_variable("m", CastToJS<decltype(m)>()(*i, m));
        c->run("assert_contents(m, {'one': 1, 'two': 2, 'three': 3});");

        std::map<std::string, int> m2{{"four", 4},{"five", 5},{"six", 6}};
        c->add_variable("m2", CastToJS<decltype(m2)>()(*i, m2));
        c->run("assert_contents(m2, {'four': 4, 'five': 5, 'six': 6});");

        std::deque<long> d{7000000000, 8000000000, 9000000000};
        c->add_variable("d", CastToJS<decltype(d)>()(*i, d));
        c->run("assert_contents(d, [7000000000, 8000000000, 9000000000]);");

        std::multimap<string, int> mm{{"a",1},{"a",2},{"a",3},{"b",4},{"c",5},{"c",6}};
        c->add_variable("mm", CastToJS<decltype(mm)>()(*i, mm));
        c->run("assert_contents(mm, {a: [1, 2, 3], b: [4], c: [5, 6]});");
        auto js_mm = c->run("mm");
        auto reconstituted_mm = CastToNative<decltype(mm)>()(*i, js_mm.Get(*i));
        assert(reconstituted_mm.size() == 6);
        assert(reconstituted_mm.count("a") == 3);
        assert(reconstituted_mm.count("b") == 1);
        assert(reconstituted_mm.count("c") == 2);

        std::array<int, 3> a{{1,2,3}};
        c->add_variable("a", CastToJS<decltype(a)>()(*i, a));
        c->run("assert_contents(a, [1, 2, 3]);");

        std::map<std::string, std::vector<int>> composite = {{"a",{1,2,3}},{"b",{4,5,6}},{"c",{7,8,9}}};
        c->add_variable("composite", CastToJS<decltype(composite)>()(*i, composite));
        c->run("assert_contents(composite, {'a': [1, 2, 3], 'b': [4, 5, 6], 'c': [7, 8, 9]});");

        {
            std::string tuple_string("Hello");
            auto tuple = make_tuple(1, 2.2, tuple_string);
            c->expose_variable("tuple", tuple);
            c->run("assert_contents(tuple, [1, 2.2, 'Hello'])");
        }
        // printf("Done testing STL container casts\n");

    });


}
