

#include <fstream>
#include <iostream>
#include <sstream>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"
#include "clang/Tooling/Tooling.h"
#pragma clang diagnostic pop

#include "../ast_action.h"
#include "../log.h"
#include "../helper_functions.h"


#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <xl/library_extensions.h>
#include <xl/templates.h>
#include <xl/log.h>
#include <xl/json.h>

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
using namespace v8toolkit::class_parser::bidirectional_output;

struct  Environment : public ::testing::Environment {
    ~Environment() override {}
    bool _expect_errors = false;
    int error_count = 0;

    decltype(LogT::statuses) statuses;

    void expect_errors() {
        assert(!this->_expect_errors);
        this->statuses = v8toolkit::class_parser::log.get_statuses();
        v8toolkit::class_parser::log.set_regex_filter(""); // must clear regex filter so expected error messages aren't filtered
        v8toolkit::class_parser::log.set_all_subjects(true);
        v8toolkit::class_parser::log.set_status(LogT::Subjects::ShouldBeWrapped, false);
        this->_expect_errors = true;
    }


    int expect_no_errors() {
        v8toolkit::class_parser::log.set_statuses(this->statuses);
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
            if (message.level == xl::templates::LogT::Levels::Error) {
                std::cerr << message.string << std::endl;
                EXPECT_EQ(message.string, "TEMPLATE LOG ERROR");
            }
        });

        // force error logging on regardless of what is in log status file because it is required for testing
        v8toolkit::class_parser::log.set_status(LogT::Levels::Error, true);
        xl::templates::log.set_status(xl::templates::LogT::Levels::Error, true);

        v8toolkit::class_parser::log.add_callback([this](LogT::LogMessage const & message) {
            if (message.level == LogT::Levels::Error) {
                if (!_expect_errors) {
                    // If this fires, it means no error message was expected
                    EXPECT_EQ(message.string, "Unexpected error message");
                } else {
                    // every error will subsequently cause an exception to be thrown, which in turn logs another error
                    //   don't count that final error
                    if (message.subject != LogT::Subjects::Exception) {
                        error_count++;
                    }
                }
            }
        });
    }
    // Override this to define how to tear down the environment.
    void TearDown() override {}

};

Environment * environment = new Environment;
static ::testing::Environment* const dummy = ::testing::AddGlobalTestEnvironment(environment);

v8toolkit::class_parser::PrintFunctionNamesAction * action = nullptr;
auto run_code(std::string source, vector<unique_ptr<OutputModule>> output_modules = {}, xl::json::Json json = xl::json::Json{}) {


    static std::string source_prefix = R"(
        #include "wrapped_class_base.h"
        #include "class_parser.h"
)";

    // vector crashes on cleanup for unknown reason so just leak it
    std::vector<std::string> args{
        "-std=c++17",
        "-I" CLANG_HOME "/include/c++/v1/",
        "-I" CLANG_HOME "/lib/clang/5.0.0/include/",

        // why doesn't this seem to get passed to the plugin?
        "-Xclang", "-plugin-arg-v8toolkit-generate-bindings", "-Xclang", "--config-file=test_plugin_config_file.json"
    };


    // there's a bug during cleanup if this object is destroyed, so just leak it
    action = new v8toolkit::class_parser::PrintFunctionNamesAction();
    action->config_data = json;
    for(auto & output_module : output_modules) {
        action->add_output_module(std::move(output_module));
    }

    // added to bypass the "no modules specified" abort rule
    if (action->output_modules.empty()) {
        action->add_output_module(std::make_unique<noop_output::NoOpOutputModule>());
    }

    // This call calls delete on action
    try {
        clang::tooling::runToolOnCodeWithArgs(action,
                                              source_prefix + source,
                                              args);
    } catch (v8toolkit::class_parser::ClassParserException & e) {
        // nothing to do here
    }

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



TEST(ClassParser, ClassMatchingJSReservedWord) {
    std::string source = R"(
        class Object : public v8toolkit::WrappedClassBase {
            static void name(); // name is reserved property on functions
        };
        class V8TOOLKIT_USE_NAME(TestMap) Map : public v8toolkit::WrappedClassBase {};
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 2); // one for Object, another for name()
}


TEST(ClassParser, DuplicateMemberFunctionName) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            void duplicated_name(int) const &;
            void duplicated_name(float) volatile &&;
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);
}


TEST(ClassParser, DuplicateStaticMemberFunctionName) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            static void duplicated_name(int);
            static void duplicated_name(float);
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);
}

TEST(ClassParser, DuplicateStaticMemberFunctionNameFixedWithJsonConfig) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            static void duplicated_name(int);
            static void duplicated_name(float);
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source, {}, xl::json::Json(R"JSON(
{
    "classes": {
        "DuplicateFunctionNameClass": {
            "members": {
                "void DuplicateFunctionNameClass::duplicated_name(int)": {
                    "name": "different name"
                }
            }
        }
    }
}
)JSON"));
    EXPECT_EQ(environment->expect_no_errors(), 0);
}

TEST(ClassParser, DuplicateStaticMemberFunctionNameFixedWithJsonConfigBySkipping) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            static void duplicated_name(int);
            static void duplicated_name(float);
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source, {}, xl::json::Json(R"JSON(
{
    "classes": {
        "DuplicateFunctionNameClass": {
            "members": {
                "void DuplicateFunctionNameClass::duplicated_name(int)": {
                    "skip": true
                }
            }
        }
    }
}
)JSON"));
    EXPECT_EQ(environment->expect_no_errors(), 0);
}




TEST(ClassParser, DuplicateDataMemberFunctionName) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            V8TOOLKIT_USE_NAME(duplicated_name) int name_one;
            V8TOOLKIT_USE_NAME(duplicated_name) int name_two;
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);
}


TEST(ClassParser, DuplicateDataMemberFunctionNameFromConfig) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            int name_one;
            int name_two;
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source, {}, xl::json::Json(R"JSON(
{
    "classes": {
        "DuplicateFunctionNameClass": {
            "members": {
                "DuplicateFunctionNameClass::name_one": {
                    "name": "duplicate_name"
                },
                "DuplicateFunctionNameClass::name_two": {
                    "name": "duplicate_name"
                }
            }
        }
    }
}
)JSON"));
    EXPECT_EQ(environment->expect_no_errors(), 1);
}

TEST(ClassParser, DuplicateDataMemberFunctionNameFromConfigButOneSkipped) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            int name_one;
            int name_two;
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source, {}, xl::json::Json(R"JSON(
{
    "classes": {
        "DuplicateFunctionNameClass": {
            "members": {
                "DuplicateFunctionNameClass::name_one": {
                    "name": "duplicate_name",
                    "skip": true
                },
                "DuplicateFunctionNameClass::name_two": {
                    "name": "duplicate_name"
                }
            }
        }
    }
}
)JSON"));
    EXPECT_EQ(environment->expect_no_errors(), 0);
}



TEST(ClassParser, DuplicateMixedMemberFunctionName) {
    std::string source = R"(
        class DuplicateFunctionNameClass : public v8toolkit::WrappedClassBase {
        public:
            V8TOOLKIT_USE_NAME(duplicated_name) int name_one;
            void duplicated_name(int) const volatile &&;
        };
    )";

    environment->expect_errors();
    auto pruned_vector = run_code(source);
    EXPECT_EQ(environment->expect_no_errors(), 1);
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

    EXPECT_EQ(c.class_name, "A");
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



struct BidirectionalTestStreamProvider : public OutputStreamProvider {

    BidirectionalTestStreamProvider() {
        BidirectionalTestStreamProvider::class_outputs.clear();
    }

    // static so the data hangs around after the run
    static inline std::map<std::string, std::stringstream> class_outputs;
    std::ostream & get_class_collection_stream() override {
        std::cerr << fmt::format("HI THERE") << std::endl;
        return std::cerr;
    }

    ostream & get_class_stream(WrappedClass const & c) override {
        return this->class_outputs[c.get_js_name()];
    }

};


TEST(ClassParser, CustomExtensions) {
    std::string source = R"(
    class V8TOOLKIT_USE_NAME(DifferentNameForClassA)  A : public v8toolkit::WrappedClassBase {
    public:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_public();
        V8TOOLKIT_CUSTOM_EXTENSION void custom_extension_public_not_static();
    protected:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_protected();
    private:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_private();
    };

)";



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


TEST(ClassParser, CustomExtensionInheritance) {
    std::string source = R"(
    class V8TOOLKIT_USE_NAME(DifferentNameForClassA)  A : public v8toolkit::WrappedClassBase {
    public:
        V8TOOLKIT_CUSTOM_EXTENSION static void custom_extension_public();
    };
    class B : public A {

    };

)";



    vector<unique_ptr<OutputModule>> output_modules;
    std::stringstream javascript_stub_string_stream;
    output_modules.push_back(std::make_unique<JavascriptStubOutputModule>(std::make_unique<StringStreamOutputStreamProvider>(javascript_stub_string_stream)));

    std::stringstream bindings_string_stream;
    output_modules.push_back(make_unique<BindingsOutputModule>(15, std::make_unique<StringStreamOutputStreamProvider>(bindings_string_stream)));

    output_modules.push_back(make_unique<BidirectionalOutputModule>(std::make_unique<BidirectionalTestStreamProvider>()));

    auto pruned_vector = run_code(source, std::move(output_modules));

    std::cerr << fmt::format("{}", bindings_string_stream.str()) << std::endl;
    EXPECT_TRUE(xl::Regex("\\{.*?add_new_constructor_function_template_callback\\(\\&A::custom_extension_public\\).*?\\}.*?\\{.*?add_new_constructor_function_template_callback\\(\\&A::custom_extension_public\\).*?\\}", xl::RegexFlags::DOTALL).match(
        bindings_string_stream.str()
    ));

}


TEST(ClassParser, Enums) {
    std::string source = R"(
    class V8TOOLKIT_USE_NAME(DifferentNameForClassA)  A : public v8toolkit::WrappedClassBase {
    public:
        enum class MyEnum{A = 0, B, C, D, E, F};
        enum class MyNonZeroBasedEnum{ G = 10, H = 15, I = 20, J = 25};
    };

)";

    vector<unique_ptr<OutputModule>> output_modules;
    std::stringstream javascript_stub_string_stream;
    output_modules.push_back(std::make_unique<JavascriptStubOutputModule>(std::make_unique<StringStreamOutputStreamProvider>(javascript_stub_string_stream)));

    std::stringstream bindings_string_stream;
    output_modules.push_back(make_unique<BindingsOutputModule>(15, std::make_unique<StringStreamOutputStreamProvider>(bindings_string_stream)));

    output_modules.push_back(make_unique<BidirectionalOutputModule>(std::make_unique<BidirectionalTestStreamProvider>()));

    auto pruned_vector = run_code(source, std::move(output_modules));

    std::cerr << fmt::format("{}", bindings_string_stream.str()) << std::endl;
    EXPECT_TRUE(xl::Regex("class_wrapper\\.add_enum\\(\"MyEnum\", \\{A, 0\\}, \\{B, 1\\}, \\{C, 2\\}, \\{D, 3\\}, \\{E, 4\\}, \\{F, 5\\}\\);", xl::RegexFlags::DOTALL).match(
        bindings_string_stream.str()
    ));
    EXPECT_TRUE(xl::Regex("class_wrapper.add_enum\\(\"MyNonZeroBasedEnum\", \\{G, 10\\}, \\{H, 15\\}, \\{I, 20\\}, \\{J, 25\\}\\);", xl::RegexFlags::DOTALL).match(
        bindings_string_stream.str()
    ));


}


TEST(ClassParser, BindingsOutputModuleConfigFileMaxDeclarationCount) {
    PrintFunctionNamesAction::config_data = xl::json::Json("{\"output_modules\": {\"BindingsOutputModule\":{\"max_declarations_per_file\":23}}}");
    BindingsOutputModule bindings_module;
    PrintFunctionNamesAction::config_data = xl::json::Json{};

    EXPECT_EQ(bindings_module.get_max_declarations_per_file(), 23);
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
    EXPECT_EQ(environment->expect_no_errors(), 2);

    EXPECT_EQ(pruned_vector.size(), 2);
    WrappedClass const & c1 = *pruned_vector[0].get();
    WrappedClass const & c2 = *pruned_vector[1].get();

    EXPECT_EQ(c1.get_constructors().size(), 1);
    EXPECT_EQ(c1.get_constructors()[0]->js_name, "TemplatedClass<int>");
    EXPECT_EQ(c2.get_constructors().size(), 1);
    EXPECT_EQ(c2.get_constructors()[0]->js_name, "TemplatedClass<char>");

    EXPECT_EQ(c1.log_watcher.errors.size(), 1);
    EXPECT_EQ(c2.log_watcher.errors.size(), 1);

    EXPECT_EQ(c1.log_watcher.errors.size() + c2.log_watcher.errors.size(), 2);

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

    (void)c2.get_js_name();
    (void)c2.get_js_name();


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
            static int static_function(char* input);

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

        using TI5 V8TOOLKIT_NAME_ALIAS = TemplatedClassWithComments<int, 5>;
        using TC6 V8TOOLKIT_NAME_ALIAS = TemplatedClassWithComments<char, 6>;

        TemplatedClassWithComments<int, 5> a;
        TemplatedClassWithComments<char, 6> b;
    )";


    auto pruned_vector = run_code(source);

    EXPECT_EQ(pruned_vector.size(), 4);
    WrappedClass const & c1 = *pruned_vector.at(0).get();
    (void) *pruned_vector.at(1).get();
    (void) *pruned_vector.at(2).get();
    (void) *pruned_vector.at(3).get();

    EXPECT_EQ(c1.get_js_name(), "ClassWithComments");
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

    namespace NameSpace {
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
    } // end namespace NameSpace
    class V8TOOLKIT_BIDIRECTIONAL_CLASS C : public NameSpace::B {
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
        static void member_static_functionC(char * p1, int p2=4);

        /// comment on data_memberC
        int data_memberC;

        virtual void this_is_a_virtual_function();

        V8TOOLKIT_USE_NAME(DIFFERENT_JS_NAME) void this_is_cpp_name();
    };

    template<typename T>
        class D : public v8toolkit::WrappedClassBase {
    };
    using d_int V8TOOLKIT_NAME_ALIAS = D<int>;
    D<int> d;
    )";


    vector<unique_ptr<OutputModule>> output_modules;
    std::stringstream javascript_stub_string_stream;
    output_modules.push_back(std::make_unique<JavascriptStubOutputModule>(std::make_unique<StringStreamOutputStreamProvider>(javascript_stub_string_stream)));

    std::stringstream bindings_string_stream;
    output_modules.push_back(make_unique<BindingsOutputModule>(15, std::make_unique<StringStreamOutputStreamProvider>(bindings_string_stream)));

    output_modules.push_back(make_unique<BidirectionalOutputModule>(std::make_unique<BidirectionalTestStreamProvider>()));

    auto pruned_vector = run_code(source, std::move(output_modules));

    EXPECT_FALSE(javascript_stub_string_stream.str().empty());

    EXPECT_FALSE(bindings_string_stream.str().empty());
    std::cerr << fmt::format("{}", bindings_string_stream.str()) << std::endl;
    EXPECT_TRUE(xl::Regex("add_member<&A::data_memberA>\\(\"data_memberA\"\\)").match(bindings_string_stream.str()));
    EXPECT_TRUE(xl::Regex("class_wrapper\\.add_static_method<void, char \\*, int>\\(\"member_static_functionC\", &C::member_static_functionC, std::tuple<int>\\(4\\)\\);").match(bindings_string_stream.str()));
    EXPECT_TRUE(xl::Regex("template class v8toolkit::V8ClassWrapper<A>;").match(bindings_string_stream.str()));
    EXPECT_TRUE(xl::Regex("template class v8toolkit::V8ClassWrapper<NameSpace::B>;").match(bindings_string_stream.str()));


    // A, B, C, JSC (JSWrapper for C - not actually present in AST, created in class parser code)
    EXPECT_EQ(pruned_vector.size(), 5); // A, B, C, D<int>, JSC (bidirectional C)
    (void)*pruned_vector[0].get();

    EXPECT_EQ(BidirectionalTestStreamProvider::class_outputs["A"].str(), "");
    EXPECT_EQ(BidirectionalTestStreamProvider::class_outputs["B"].str(), "");
    EXPECT_EQ(BidirectionalTestStreamProvider::class_outputs["C"].str(), "");
    EXPECT_EQ(BidirectionalTestStreamProvider::class_outputs["JSC"].str(), R"(#pragma once

#include "v8toolkit_generated_bidirectional_C.h"
#include <v8toolkit/bidirectional.h>

class JSC : public C, public v8toolkit::JSWrapper<C> {
public:

    JSC(v8::Local<v8::Context> context, v8::Local<v8::Object> object,
        v8::Local<v8::FunctionTemplate> created_by, ) :
      C(),
      v8toolkit::JSWrapper<C>(context, object, created_by)
    {}

        JS_ACCESS_00(void, this_is_a_virtual_function);
})");}



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
    EXPECT_FALSE(string_stream.str().empty());
    EXPECT_FALSE(xl::Regex("operator\\(\\)").match(string_stream.str()));

}


int main(int argc, char* argv[]) {
testing::InitGoogleTest(&argc, argv);
return RUN_ALL_TESTS();
}


// test parsing class, setting typedef name, then parisng methods (incuding constructors) to make sure constructor picks up typedef name
