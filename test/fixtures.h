#pragma once

#include "javascript.h"
#include "v8toolkit.h"

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
    JavaScriptFixture() {
        i = v8toolkit::Platform::create_isolate();
        i->add_assert();
    }

    void create_context() {
        c = i->create_context();

        // does a does a deep comparison between the two objects passed and if they don't match, it prints out the info
        //   and then fails a GoogleTest EXPECT_EQ match.  Just using EXPECT_EQ works, but you can't print out what the
        //   comparison was
        c->add_function("EXPECT_EQJS", [this](v8::Local<v8::Value> lhs, v8::Local<v8::Value> rhs) {
            if (!v8toolkit::compare_contents(*i, lhs, rhs)) {
                std::cerr << fmt::format("EXPECT_EQJS failed: {} != {}", *v8::String::Utf8Value(lhs), *v8::String::Utf8Value(rhs)) << std::endl;
                EXPECT_TRUE(v8toolkit::compare_contents(*i, lhs, rhs));
            }
        });

        c->add_function("EXPECT_TRUE", [](bool expected_true_value) {
            EXPECT_TRUE(expected_true_value);
        });
    }

};
