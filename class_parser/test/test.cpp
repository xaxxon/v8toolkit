

#include <fstream>
#include <sstream>

#include "../clang.h"
#include "../ast_action.h"


#include <gtest/gtest.h>
#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;

TEST(ClassParser, ClassParser) {
    std::ifstream sample_source("sample.cpp");
    std::string sample_source_contents((std::istreambuf_iterator<char>(sample_source)),
                                       std::istreambuf_iterator<char>());

    std::vector<std::string> args = {
        "-std=c++17",
        "-I" CLANG_HOME "/include/c++/v1/",
        "-I" CLANG_HOME "/lib/clang/5.0.0/include/"
    };

    std::cout << "Printing args:" << std::endl;
    for(auto const & str : args) {
        std::cout << str << std::endl;
    }
    std::cout << "Done printing args" << std::endl;

    // there's a bug during cleanup if this object is destroyed, so just leak it
    clang::tooling::runToolOnCodeWithArgs(new v8toolkit::class_parser::PrintFunctionNamesAction,
                                  sample_source_contents,
                                  args);
}


int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}