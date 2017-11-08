

#include <fstream>
#include <iostream>
#include <sstream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/Tooling/Tooling.h"
#pragma clang diagnostic pop

#include "../ast_action.h"
#include "../log.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <xl/library_extensions.h>
#include <xl/templates.h>
#include <xl/log.h>

using namespace xl;
using namespace std;

#include "../output_modules/noop_output.h"
#include "../output_modules/javascript_stub_output.h"
//#include "../output_modules/bindings_output.h"
#include "../output_modules/bidirectional_output.h"



using ::testing::_;
using ::testing::Return;
using namespace v8toolkit::class_parser;
using namespace v8toolkit::class_parser::javascript_stub_output;
using namespace v8toolkit::class_parser::bindings_output;

struct  Environment : public ::testing::Environment {
    ~Environment() override {}
    bool _expect_errors = false;
    int error_count = 0;

    std::vector<char> subject_status_backup;
    std::vector<char> level_status_backup;

    void expect_errors() {
        assert(!this->_expect_errors);
        this->subject_status_backup = v8toolkit::class_parser::log.get_status_of_subjects();
        this->level_status_backup = v8toolkit::class_parser::log.get_status_of_levels();

        v8toolkit::class_parser::log.set_all_subjects(true);
        this->_expect_errors = true;
    }


    int expect_no_errors() {
        v8toolkit::class_parser::log.set_status_of_subjects(this->subject_status_backup);
        v8toolkit::class_parser::log.set_status_of_levels(this->level_status_backup);
//        std::cerr << fmt::format("restoring subject status: {}", xl::join(subject_status_backup)) << std::endl;
        assert(this->_expect_errors);
        this->_expect_errors = false;
        int result = error_count;
        error_count = 0;
        return result;
    }


    // Override this to define how to set up the environment.
    void SetUp() override {
        xl::templates::log.add_callback([](xl::templates::LogT::LogMessage const & message) {
            std::cerr << fmt::format("xl::templates: {}", message.string) << std::endl;
            EXPECT_EQ(message.string, "TEMPLATE LOG ERROR");
        });

        // force error logging on regardless of what is in log status file because it is required for testing
        v8toolkit::class_parser::log.enable_status_file("class_parser_plugin.log_status");
        v8toolkit::class_parser::log.set_level_status(LogT::Levels::Levels::Error, true);


        v8toolkit::class_parser::log.add_callback([this](LogT::LogMessage const & message) {
            if (message.level == LogT::Levels::Levels::Error) {
                if (!_expect_errors) {
                    std::cerr << fmt::format("class_parser error message: {}", message.string) << std::endl;
                    EXPECT_EQ(message.string, "CLASS PARSER LOG ERROR");
                } else {
                    error_count++;
                }
            } else {
                // if other log levels are being handled, just print it out
                std::cout << fmt::format("{}: {}", message.log.get_subject_name(message.subject), message.string) << std::endl;
            }
        });
    }
    // Override this to define how to tear down the environment.
    void TearDown() override {}

};

Environment * environment = new Environment;
static ::testing::Environment* const dummy = ::testing::AddGlobalTestEnvironment(environment);

v8toolkit::class_parser::PrintFunctionNamesAction * action = nullptr;
auto run_code(std::string source, vector<unique_ptr<OutputModule>> output_modules = {}) {


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
    action = new v8toolkit::class_parser::PrintFunctionNamesAction();
    for(auto & output_module : output_modules) {
        action->add_output_module(std::move(output_module));
    }

    // added to bypass the "no modules specified" abort rule
    if (action->output_modules.empty()) {
        action->add_output_module(std::make_unique<noop_output::NoOpOutputModule>());
    }

//    std::cerr << fmt::format("STARTING A NEW RUN") << std::endl;

    // This call calls delete on action
    clang::tooling::runToolOnCodeWithArgs(action,
                                          source_prefix + source,
                                          args);

    return  erase_if(copy(WrappedClass::wrapped_classes), [](std::unique_ptr<WrappedClass> const & c){return !c->should_be_wrapped();});
}

TEST(ClassParser, ClassParser) {

    std::string source = R"(
        struct SimpleWrappedClass : public v8toolkit::WrappedClassBase {
            SimpleWrappedClass(){}
};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();
    EXPECT_EQ(c.get_short_name(), "SimpleWrappedClass");

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);

    EXPECT_EQ(c.get_constructors().size(), 1);
}

TEST(ClassParser, DoNotWrapConstructors) {

    std::string source = R"(
        struct V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS SimpleWrappedClass : public v8toolkit::WrappedClassBase {};
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();

    EXPECT_EQ(c.get_members().size(), 0);
    EXPECT_EQ(c.get_member_functions().size(), 0);
    EXPECT_EQ(c.get_static_functions().size(), 0);
    EXPECT_EQ(c.wrapper_custom_extensions.size(), 0);

    EXPECT_EQ(c.get_constructors().size(), 0);

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




TEST(ClassParser, JSDocTypeNames) {
    std::string source = R"(
        #include <vector>
        struct B {};

        struct A : public v8toolkit::WrappedClassBase {
          public:
            std::vector<int> & do_something(int & i){static std::vector<int> v; return v;}
            B & b_func(B & b1, B && b2){return b1;}
        };
    )";


    auto pruned_vector = run_code(source);
    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();

    EXPECT_EQ(c.get_member_functions().size(), 2);
    auto & mf = *c.get_member_functions()[0];
    auto & mf2 = *c.get_member_functions()[1];

    EXPECT_EQ(mf.parameters.size(), 1);
    EXPECT_EQ(mf.parameters[0].type.get_jsdoc_type_name(), "Number");
    EXPECT_EQ(mf.return_type.get_jsdoc_type_name(), "Array.{Number}");

    EXPECT_EQ(mf2.parameters.size(), 2);
    EXPECT_EQ(mf2.return_type.get_jsdoc_type_name(), "B");
    EXPECT_EQ(mf2.parameters[0].type.get_jsdoc_type_name(), "B");
    EXPECT_EQ(mf2.parameters[1].type.get_jsdoc_type_name(), "B");



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

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);


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

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);

    EXPECT_EQ(pruned_vector.size(), 2);
    WrappedClass const & c1 = *pruned_vector[0].get();
    WrappedClass const & c2 = *pruned_vector[1].get();

    EXPECT_EQ(c1.get_constructors().size(), 1);
    EXPECT_EQ(c1.get_constructors()[0]->js_name, "TemplatedClass");
    EXPECT_EQ(c2.get_constructors().size(), 1);
    EXPECT_EQ(c2.get_constructors()[0]->js_name, "TemplatedClass");

    environment->expect_errors();
    (void)c1.get_jsdoc_name();
    (void)c2.get_jsdoc_name();
    EXPECT_EQ(environment->expect_no_errors(), 2);


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

    (void)c2.get_jsdoc_name();
    (void)c2.get_jsdoc_name();


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
        class V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS TemplatedClassWithComments : public v8toolkit::WrappedClassBase {};

        TemplatedClassWithComments<int, 5> a;
        TemplatedClassWithComments<char, 6> b;
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 4);
    WrappedClass const & c1 = *pruned_vector.at(0).get();
    (void) *pruned_vector.at(1).get();
    (void) *pruned_vector.at(2).get();
    (void) *pruned_vector.at(3).get();

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




TEST(ClassParser, ClassComments) {
    std::string source = R"(
    class V8TOOLKIT_DO_NOT_WRAP_CONSTRUCTORS A : public v8toolkit::WrappedClassBase {
    public:
        void member_instance_functionA();
        static void member_static_functionA();

        /// comment on data_memberA
        int data_memberA;
    };

    /**
     * This is a comment on class B
     */
    class B : public v8toolkit::WrappedClassBase {
    public:

        /**
         * Construct a B from a string
         * @param string_name the name for creating B with
         */
        B(char const * string_name = "default string name");

        void member_instance_functionB();
        static void member_static_functionB();

        /// comment on data_memberB
        int data_memberB;
    };
    class V8TOOLKIT_BIDIRECTIONAL_CLASS C : public B {
    public:

        V8TOOLKIT_BIDIRECTIONAL_CONSTRUCTOR
        C(){}

        /**
          * member instance function C comment
          * @param p1 some string parametere
          * @param p2 some number parameter
          * @return some number returned
          */
        int member_instance_functionC(char * p1, int p2);

        void member_function_no_params();

        /**
          * static instance function C comment
          * @param p1 static some string parametere
          * @param p2 static some number parameter
          * @return static some number returned
          */
        static void member_static_functionC(char * p1, int p2);

        /// comment on data_memberC
        int data_memberC;
    };

//    template<typename T>
//        class D : public v8toolkit::WrappedClassBase {
//    };
//    D<int> d;
    )";


    vector<unique_ptr<OutputModule>> output_modules;
    output_modules.push_back(make_unique<JavascriptStubOutputModule>());
    output_modules.push_back(make_unique<BindingsOutputModule>(15));
    output_modules.push_back(make_unique<BidirectionalOutputModule>());
//    output_modules.push_back(make_unique<BidirectionalOutputModule>());

    auto pruned_vector = run_code(source, std::move(output_modules));

    // A, B, C, JSC (JSWrapper for C - not actually present in AST, created in class parser code)
    EXPECT_EQ(pruned_vector.size(), 4);
    (void)*pruned_vector[0].get();
}



TEST(ClassParser, CallableOverloadFilteredFromJavascriptStub) {
    std::string source = R"(
    class A : public v8toolkit::WrappedClassBase {
    public:
        void operator()(){};
    };
    )";

    vector<std::unique_ptr<OutputModule>> output_modules;
    std::stringstream string_stream;
    output_modules.push_back(std::make_unique<JavascriptStubOutputModule>(std::make_unique<StringStreamOutputStreamProvider>(string_stream)));
    auto pruned_vector = run_code(source, std::move(output_modules));

    EXPECT_EQ(pruned_vector.size(), 1);
    WrappedClass const & c = *pruned_vector[0].get();

    EXPECT_EQ(c.get_member_functions().size(), 1);
    EXPECT_FALSE(xl::Regex("operator\\(\\)").match(string_stream.str()));

}



//TEST(ClassParser, ClassComments) {
//    std::string source = R"(
//    class A : public v8toolkit::WrappedClassBase {
//      public:
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