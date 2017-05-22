#include "testing.h"

#include "javascript.h"

using namespace testing;

int main(int argc, char* argv[]) {

    v8toolkit::Platform::init(argc, argv, "../");
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}