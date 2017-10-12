

#include <fstream>
#include <iostream>
#include <sstream>

#include "../clang.h"
#include "../ast_action.h"


#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <xl/library_extensions.h>
using namespace xl;
using namespace std;

#include "../output_modules/javascript_stub_output.h"

using ::testing::_;
using ::testing::Return;
using namespace v8toolkit::class_parser;

TEST(ClassParser, ClassParser) {
//    std::ifstream sample_source("sample.cpp");
//    assert(sample_source.is_open());
//
//
//    std::string sample_source_contents((std::istreambuf_iterator<char>(sample_source)),
//                                       std::istreambuf_iterator<char>());

std::string source = R"(
    #include "wrapped_class_base.h"
    class SimpleWrappedClass : public v8toolkit::WrappedClassBase {};
)";

    // vector crashes on cleanup for unknown reason so just leak it
    std::vector<std::string> args{
        "-std=c++17",
        "-I" CLANG_HOME "/include/c++/v1/",
        "-I" CLANG_HOME "/lib/clang/5.0.0/include/"
    };


    // there's a bug during cleanup if this object is destroyed, so just leak it
    auto action = new v8toolkit::class_parser::PrintFunctionNamesAction;


    clang::tooling::runToolOnCodeWithArgs(action,
                                          source,
                                          args);

    auto pruned_vector = erase_if(copy(WrappedClass::wrapped_classes), [](WrappedClass const & c){return !c.should_be_wrapped();});

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);

}


int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}