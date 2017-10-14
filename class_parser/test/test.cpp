

#include <fstream>
#include <iostream>
#include <sstream>

#include "../clang.h"
#include "../ast_action.h"
#include "../log.h"

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

//    std::cerr << fmt::format("STARTING A NEW RUN") << std::endl;
    clang::tooling::runToolOnCodeWithArgs(action,
                                          source_prefix + source,
                                          args);

    return  erase_if(copy(WrappedClass::wrapped_classes), [](std::unique_ptr<WrappedClass> const & c){return !c->should_be_wrapped();});
}

TEST(ClassParser, ClassParser) {

    std::string source = R"(
        class SimpleWrappedClass : public v8toolkit::WrappedClassBase {};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();

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
    WrappedClass const & c = *pruned_vector[0].get();

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
    WrappedClass const & c = *pruned_vector[0].get();

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
    WrappedClass const & c = *pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);

    // only the public static entry should actually make it through
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 1);
    EXPECT_EQ(c.log_watcher.errors.size(), 1);
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
    WrappedClass const & c = *pruned_vector[0].get();

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
    EXPECT_EQ(c.get_members().at(0)->js_name, "data_member");

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
    WrappedClass const & c = *pruned_vector[0].get();

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

        TemplatedClass<int> a;
        TemplatedClass<char> b;
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 2);
    WrappedClass const & c1 = *pruned_vector[0].get();
    WrappedClass const & c2 = *pruned_vector[1].get();

    EXPECT_EQ(c1.get_constructors().size(), 1);
    EXPECT_EQ(c1.get_constructors()[0]->js_name, "TemplatedClass");
    EXPECT_EQ(c2.get_constructors().size(), 1);
    EXPECT_EQ(c2.get_constructors()[0]->js_name, "TemplatedClass");

    // expecting error because both instantiations of TemplatedClass will have the same constructor name
    //   but the error only shows up on the second class because it's still unique when the first one is
    //   processed
    EXPECT_EQ(c1.log_watcher.errors.size() + c2.log_watcher.errors.size(), 1);

}


TEST(ClassParser, TemplatedClassInstantiationsSetJavascriptNameViaUsingNameAlias) {
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
    WrappedClass const & c1 = *pruned_vector[0].get();
    WrappedClass const & c2 = *pruned_vector[1].get();

    EXPECT_EQ(c1.get_constructors().size(), 1);
    EXPECT_TRUE(c1.get_constructors()[0]->js_name == "A" || c1.get_constructors()[0]->js_name == "B");
    EXPECT_EQ(c2.get_constructors().size(), 1);
    EXPECT_TRUE(c2.get_constructors()[0]->js_name == "A" || c2.get_constructors()[0]->js_name == "B");
    EXPECT_NE(c1.get_constructors()[0]->js_name, c2.get_constructors()[0]->js_name);

    // no conflicting constructor name because the name_alias on each type will rename the constructor of each
    //   instantiation as well
    EXPECT_EQ(c1.log_watcher.errors.size() + c2.log_watcher.errors.size(), 0);
}


TEST(ClassParser, AbstractClass) {
    std::string source = R"(
        class AbstractClass : public v8toolkit::WrappedClassBase{
        public:
            AbstractClass(){}
            virtual void pure_virtual_function() = 0;
        };
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();

    // no reason to deal with constructors of abstract types since they won't be constructed directly
    EXPECT_EQ(c.get_constructors().size(), 0);

    EXPECT_EQ(c.get_member_functions().size(), 1);

}




TEST(ClassParser, ClassAndFunctionComments) {
    std::string source = R"(
        /**
         * Class Description
         * and more class description
         */
        class ClassWithComments : public v8toolkit::WrappedClassBase {
        public:
            /**
             * function description
             * @param input input description
             * @param input2 this is a longer, multiline
             *   comment which should all be captured
             * @return return description
             */
            int function(char* input, int input2);

            /**
             * static function description
             * @param input static function input description
             * @return static function return description
             */
            int static static_function(char* input);

            /**
             * data member description
             */
            int data_member;
        };

        /// Short class comment
        class AnotherClassWithComments : public v8toolkit::WrappedClassBase {};

        /**
         * TemplatedClassWithComments description
         * @tparam T template type parameter
         * @tparam i template non-type parameter
         */
        template<class T, int i>
        class TemplatedClassWithComments : public v8toolkit::WrappedClassBase {};

        TemplatedClassWithComments<int, 5> a;
        TemplatedClassWithComments<char, 6> b;
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 4);
    WrappedClass const & c1 = *pruned_vector.at(0).get();
    WrappedClass const & c2 = *pruned_vector.at(1).get();
    WrappedClass const & c3 = *pruned_vector.at(2).get();
    WrappedClass const & c4 = *pruned_vector.at(3).get();

    EXPECT_EQ(c1.get_name_alias(), "ClassWithComments");
    EXPECT_EQ(c1.get_member_functions().size(), 1);
    EXPECT_EQ(c1.comment, "Class Description and more class description");
    MemberFunction const & member_function = *c1.get_member_functions()[0];
    EXPECT_EQ(member_function.return_type_comment, "return description");
    EXPECT_EQ(member_function.parameters.size(), 2);
    {
        ClassFunction::ParameterInfo const & parameter = member_function.parameters.at(0);
        EXPECT_EQ(parameter.name, "input");
        EXPECT_EQ(parameter.type.get_name(), "char *");
        EXPECT_EQ(parameter.description, "input description");
    }
    {
        ClassFunction::ParameterInfo const & parameter = member_function.parameters.at(1);
        EXPECT_EQ(parameter.name, "input2");
        EXPECT_EQ(parameter.type.get_name(), "int");
        EXPECT_EQ(parameter.description, "this is a longer, multiline comment which should all be captured");
    }
    {
        auto & member = c1.get_members().at(0);
        EXPECT_EQ(member->type.get_name(), "int");
        EXPECT_EQ(member->comment, "data member description");
    }
}




TEST(ClassParser, SkipClassEvenThoughInheritsFromWrappedClassBase) {
    std::string source = R"(
class V8TOOLKIT_SKIP AnnotatedNotToBeWrapped : public v8toolkit::WrappedClassBase {};
    )";

    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 0);
}


TEST(ClassParser, V8TOOLKIT_SKIP_TESTS) {
    std::string source = R"(
class ClassWithSomeMethodsMarkedToBeSkipped : public v8toolkit::WrappedClassBase {
public:
    V8TOOLKIT_SKIP void skip_this_method();
    void do_not_skip_this_method();

    V8TOOLKIT_SKIP static void skip_this_static_method();
    static void do_not_skip_this_static_method();

    V8TOOLKIT_SKIP int skip_this_data_member;
    int do_not_skip_this_data_member;
};
    )";

    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector.at(0).get();
    EXPECT_EQ(c.get_member_functions().size(), 1);
    EXPECT_EQ(c.get_static_functions().size(), 1);
    EXPECT_EQ(c.get_members().size(), 1);
}





//TEST(ClassParser, ClassComments) {
//    std::string source = R"(
//
//    )";
//
//
//    auto pruned_vector = run_code(source);
//
//    EXPECT_EQ(pruned_vector.size(), 1);
//    WrappedClass const & c = *pruned_vector[0].get();
//}





int main(int argc, char* argv[]) {
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();
}