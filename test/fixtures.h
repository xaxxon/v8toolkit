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

class PlatformFixture : public ::testing::Test {
public:
    PlatformFixture() {

    }
};

