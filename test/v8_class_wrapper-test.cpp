#include <v8_class_wrapper.h>
#include "testing.h"

using namespace v8toolkit;

class TypeCheckerTest_ParentClass {};
class TypeCheckerTest_ChildClass : public TypeCheckerTest_ParentClass {};

TEST(TypeChecker, TypeChecker) {

    EXPECT_EQ(1,1);
}
