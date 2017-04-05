#pragma once

#include "testing.h"


class ExampleFixture : public ::testing::Test {
public:

    ExampleFixture()
    {}

	~ExampleFixture() {}

    virtual void SetUp() {
    }

};


class TypeCheckerFixture : public ::testing::Test {

public:
    TypeCheckerFixture() {

    }


};