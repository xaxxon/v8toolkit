#pragma once

#include "testing.h"


class ExampleFixture : public ::testing::Test {
public:
    k_time();

    WorkflowFixture()
    {}

	~WorkflowFixture() {}

    virtual void SetUp() {
    }

};
