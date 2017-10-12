

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


auto run_code(std::string source) {

    static std::string source_prefix = R"(
        #include "wrapped_class_base.h"
        #include "class_parser.h"
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
                                          source_prefix + source,
                                          args);

    return  erase_if(copy(WrappedClass::wrapped_classes), [](WrappedClass const & c){return !c.should_be_wrapped();});

}

TEST(ClassParser, ClassParser) {

    std::string source = R"(
        class SimpleWrappedClass : public v8toolkit::WrappedClassBase {};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);

}

TEST(ClassParser, WrappedClassWithUnwrappedBaseClasses) {

    std::string source = R"(
        class NotWrappedBase {};
        class NotWrapped : public NotWrappedBase {};
        class WrappedButParentNot : public NotWrapped, public v8toolkit::WrappedClassBase {};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 3);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);

}

TEST(ClassParser, ExplicitIgnoreBaseClass) {
    std::string source = R"(
        class NotWrappedBaseSkip {};
        class NotWrappedSkip : public NotWrappedBaseSkip {};
        class V8TOOLKIT_IGNORE_BASE_TYPE(NotWrappedSkip) WrappedButParentNot_NOSKIP : public NotWrappedSkip, public v8toolkit::WrappedClassBase {};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);

}

TEST(ClassParser, CustomExtensions) {
    std::string source = R"(
    class A : public v8toolkit::WrappedClassBase {
    public:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_public();
        V8TOOLKIT_CUSTOM_EXTENSION void custom_extension_public_not_static();
    protected:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_protected();
    private:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_private();
    };)";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);

    // only the public static entry should actually make it through
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 1);

}


TEST(ClassParser, ClassElements) {
    std::string source = R"(
    class A : public v8toolkit::WrappedClassBase {
    public:
        void member_instance_function();
        static void member_static_function();
        int data_member;
    };)";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_member_functions().size(), 1);
//    for(auto & f : c.get_member_functions()) {
//        std::cerr << fmt::format("member function: {}", f->name) << std::endl;
//    }
    EXPECT_EQ((*c.get_member_functions().begin())->js_name, "member_instance_function");

    EXPECT_EQ(c.get_static_functions().size(), 1);
//    for(auto & f : c.get_static_functions()) {
//        std::cerr << fmt::format("static function: {}", f->js_name) << std::endl;
//    }

    EXPECT_EQ((*c.get_static_functions().begin())->js_name, "member_static_function");

    EXPECT_EQ(c.get_members().size(), 1);
    EXPECT_EQ((*c.get_members().begin())->js_name, "data_member");

    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);
}



TEST(ClassParser, UseDifferentName) {
    std::string source = R"(
    class A : public v8toolkit::WrappedClassBase {
    public:
        V8TOOLKIT_USE_NAME(alternate_member_instance_function) void member_instance_function();
        V8TOOLKIT_USE_NAME(alternate_member_static_function) static void member_static_function();
        V8TOOLKIT_USE_NAME(alternate_data_member) int data_member;
    };)";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = pruned_vector[0].get();

    EXPECT_EQ(c.get_member_functions().size(), 1);
    EXPECT_EQ((*c.get_member_functions().begin())->js_name, "alternate_member_instance_function");

    EXPECT_EQ(c.get_static_functions().size(), 1);
    EXPECT_EQ((*c.get_static_functions().begin())->js_name, "alternate_member_static_function");

    EXPECT_EQ(c.get_members().size(), 1);
    EXPECT_EQ((*c.get_members().begin())->js_name, "alternate_data_member");
    // only the public static entry should actually make it through
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);
}


TEST(ClassParser, TemplatedClassInstantiations) {
    std::string source = R"(
        template<class T>
        class TemplatedClass : public v8toolkit::WrappedClassBase {};
        using A V8TOOLKIT_NAME_ALIAS = TemplatedClass<int>;
        using B V8TOOLKIT_NAME_ALIAS = TemplatedClass<char>;

        A a;
        B b;
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 2);
    WrappedClass const & c1 = pruned_vector[0].get();
    WrappedClass const & c2 = pruned_vector[1].get();

    EXPECT_EQ(c1.get_constructors().size(), 1);
    EXPECT_TRUE(c1.get_constructors()[0]->js_name == "A" || c1.get_constructors()[0]->js_name == "B");
    EXPECT_EQ(c2.get_constructors().size(), 1);
    EXPECT_TRUE(c2.get_constructors()[0]->js_name == "A" || c2.get_constructors()[0]->js_name == "B");
    EXPECT_NE(c1.get_constructors()[0]->js_name, c2.get_constructors()[0]->js_name);
}





int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}