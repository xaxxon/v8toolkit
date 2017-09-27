

#include <fstream>
#include <sstream>

#include "../clang.h"
#include "../ast_action.h"


#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../output_modules/javascript_stub_output.h"

using ::testing::_;
using ::testing::Return;

TEST(ClassParser, ClassParser) {
    std::ifstream sample_source("sample.cpp");
    std::string sample_source_contents((std::istreambuf_iterator<char>(sample_source)),
                                       std::istreambuf_iterator<char>());

    // vector crashes on cleanup for unknown reason so just leak it
    std::vector<std::string> args{
        "-std=c++17",
        "-I" CLANG_HOME "/include/c++/v1/",
        "-I" CLANG_HOME "/lib/clang/5.0.0/include/"
    };


    // there's a bug during cleanup if this object is destroyed, so just leak it
    auto action = new v8toolkit::class_parser::PrintFunctionNamesAction;

    //1action->add_output_module(std::make_unique<v8toolkit::class_parser::JavascriptStubOutput>(std::make_unique<std::ofstream>("js-api.js")));

    clang::tooling::runToolOnCodeWithArgs(action,
                                          sample_source_contents,
                                          args);

}


int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}