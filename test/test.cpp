#include "testing.h"

#include "javascript.h"

using namespace testing;

int main(int argc, char* argv[]) {
    std::cerr << fmt::format("platform::init") << std::endl;
    v8toolkit::Platform::init(argc, argv, "../");
    std::cerr << fmt::format("platform::init done") << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}