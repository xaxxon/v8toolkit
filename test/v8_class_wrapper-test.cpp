#include "testing.h"



TEST_F(JavaScriptFixture, NumberTypes) {
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

TEST_F(JavaScriptFixture, StringTypes) {
    this->create_context();

    (*c)([&] {

        EXPECT_EQ(CastToNative<std::string>()(*i, CastToJS<std::string>()(*i, "test string")), "test string");

        // These return unique_ptr<char[]> because otherwise there wouldn't be any memory for the char * to point tos
        EXPECT_STREQ(CastToNative<char *>()(*i, CastToJS<char const *>()(*i, "test string")).get(), "test string");
        EXPECT_STREQ(CastToNative<char const *>()(*i, CastToJS<char const *>()(*i, "test string")).get(), "test string");
    });
}


template<class T>
struct FlaggedDeleter {
    static bool deleted;
    void operator()(T * t) {
        this->deleted = true;
        delete t;
    }
};

template<class T>
bool FlaggedDeleter<T>::deleted = false;

TEST_F(JavaScriptFixture, UniquePointer_UnwrappedTypes) {
    this->create_context();
    (*c)([&] {
        {
            FlaggedDeleter<std::string>::deleted = false;
            auto upi = unique_ptr<std::string, FlaggedDeleter<std::string>>(new std::string("test string"));
            auto object = CastToJS<decltype(upi)>()(*i, std::move(upi));


            // CastToJS should have taken ownership of the unique_ptr and deleted its memory because it's
            //   an unwrapped type
            EXPECT_TRUE(FlaggedDeleter<std::string>::deleted);

            // string should have been moved out of
            EXPECT_STREQ(CastToNative<std::string>()(*i, object).c_str(), "test string");

        }
        {
            FlaggedDeleter<std::string>::deleted = false;
            auto upi = unique_ptr<std::string, FlaggedDeleter<std::string>>(new std::string("test string"));
            auto && rrupi = upi;
            auto object = CastToJS<decltype(upi)>()(*i, std::move(rrupi));



            // CastToJS should have taken ownership of the unique_ptr and deleted its memory because it's
            //   an unwrapped type
            EXPECT_TRUE(FlaggedDeleter<std::string>::deleted);

            // string should have been moved out of
            EXPECT_STREQ(CastToNative<std::string>()(*i, object).c_str(), "test string");

        }
        {
            {
                FlaggedDeleter<std::string>::deleted = false;
                auto upi = unique_ptr<std::string, FlaggedDeleter<std::string>>(new std::string("test string"));
                auto object = CastToJS<decltype(upi) & >()(*i, std::move(upi));

                // string should NOT have been moved out of
                EXPECT_STREQ(upi->c_str(), "test string");
                EXPECT_STREQ(CastToNative<std::string>()(*i, object).c_str(), "test string");


                // sending in unique_ptr by lvalue ref shouldn't do anything to the unique_ptr
                EXPECT_FALSE(FlaggedDeleter<std::string>::deleted);
            }
            // the unique_ptr goes out of scope and deletes as normal
            EXPECT_TRUE(FlaggedDeleter<std::string>::deleted);

        }
    });
}



TEST_F(JavaScriptFixture, Vectors) {

    this->create_context();

    (*c)([&] {
        {
            std::vector<std::string> v{"hello", "there", "this", "is", "a", "vector"};
            c->add_variable("v", CastToJS<decltype(v)>()(*i, v));
            c->run("assert_contents(v, ['hello', 'there', 'this', 'is', 'a', 'vector'])");
        }
        {
            std::vector<std::string> const cv{"hello", "there", "this", "is", "a", "vector"};
            c->add_variable("cv", CastToJS<decltype(cv)>()(*i, cv));
            c->run("assert_contents(cv, ['hello', 'there', 'this', 'is', 'a', 'vector'])");
        }
        {
            // non-wrapped element type, so the original vector remains - no new object of the Element type to move
            //   them into.
            std::vector<std::string> v{"hello", "there", "this", "is", "a", "vector"};
            c->add_variable("v", CastToJS<decltype(v)>()(*i, std::move(v)));
            c->run("assert_contents(v, ['hello', 'there', 'this', 'is', 'a', 'vector'])");
        }




        // vector
        {
            auto js_vector = c->run("[`a`, `b`, `c`]");
            auto vector = CastToNative<std::vector<std::string>>()(*i, js_vector.Get(*i));
            EXPECT_EQ(vector.size(), 3);
            EXPECT_EQ(vector[2], "c");
        }
        // const vector
        {
            auto js_vector = c->run("[`a`, `b`, `c`]");
            auto vector = CastToNative<std::vector<std::string> const>()(*i, js_vector.Get(*i));
            EXPECT_EQ(vector.size(), 3);
            EXPECT_EQ(vector[2], "c");
        }

    });
}


/**
 * Move each of these into its own test for its container type
 */
TEST_F(JavaScriptFixture, ContainerTypes) {

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
