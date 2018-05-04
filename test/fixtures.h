#pragma once

#include "v8toolkit/javascript.h"
#include "v8toolkit/v8toolkit.h"
#include "v8toolkit/v8helpers.h"
#include "testing.h"

class ExampleFixture : public ::testing::Test {
public:

    ExampleFixture()
    {}

	~ExampleFixture() {}

    virtual void SetUp() {
    }

};

class PlatformFixture : public ::testing::Test {
public:
    PlatformFixture() {

    }
};


class JavaScriptFixture : public PlatformFixture {

public:
    v8toolkit::IsolatePtr i;
    v8toolkit::ContextPtr c;
    v8::Isolate * isolate;
    JavaScriptFixture() {
        i = v8toolkit::Platform::create_isolate();
        i->add_print();
        i->add_assert();
    }

    void create_context() {
        c = i->create_context();
        isolate = i->get_isolate();

        // does a does a deep comparison between the two objects passed and if they don't match, it prints out the info
        //   and then fails a GoogleTest EXPECT_EQ match.  Just using EXPECT_EQ works, but you can't print out what the
        //   comparison was
        c->add_function("EXPECT_EQJS", [](v8::Local<v8::Value> lhs, v8::Local<v8::Value> rhs) {
            if (!v8toolkit::compare_contents(lhs, rhs)) {
                std::cerr << fmt::format("EXPECT_EQJS failed: {} != {}", *v8::String::Utf8Value(lhs), *v8::String::Utf8Value(rhs)) << std::endl;
                std::cerr << fmt::format("{}", v8toolkit::stringify_value(lhs)) << std::endl;
                std::cerr << fmt::format("{}", v8toolkit::stringify_value(rhs)) << std::endl;
                EXPECT_TRUE(v8toolkit::compare_contents(lhs, rhs));
            }
        });

        c->add_function("EXPECT_TRUE", [](bool expected_true_value) {
            if (expected_true_value == false) {
                std::cerr << fmt::format("put breakpoint here") << std::endl;
            }
            EXPECT_TRUE(expected_true_value);
        });
    }

};
