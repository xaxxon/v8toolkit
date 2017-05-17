#pragma once

#include "javascript.h"

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

        c->add_function("EXPECT_TRUE", [](bool expected_true_value) {
            EXPECT_TRUE(expected_true_value);
        });
    }

};
