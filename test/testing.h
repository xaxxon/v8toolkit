#pragma once

#include <iostream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;


#include "mocks.h"
#include "fixtures.h"

using namespace std;

#include <v8_class_wrapper.h>
#include "javascript.h"


using namespace v8toolkit;
using std::unique_ptr;
using std::make_unique;
