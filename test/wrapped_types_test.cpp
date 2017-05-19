
#include <v8_class_wrapper.h>
#include <javascript.h>
#include "wrapped_class_base.h"
#include "testing.h"


using namespace v8toolkit;
using std::unique_ptr;
using std::make_unique;


class CopyableWrappedClass : public WrappedClassBase {
public:
    CopyableWrappedClass(){}
    CopyableWrappedClass(CopyableWrappedClass const &) = default;

};



class WrappedClass : public WrappedClassBase {
public:
    int i = 5;
    int ci = 5;
    std::unique_ptr<float> upf = std::make_unique<float>(3.5);
    std::unique_ptr<float> cupf = std::make_unique<float>(4.5);

    std::string string = "string value";

    int takes_int_5(int x) {
        EXPECT_EQ(x, 5);
        return x;
    }
    int takes_const_int_6(int const x) const {
        EXPECT_EQ(x, 6);
        return x;
    }

    void default_parameters(int i = 1, char const * s = "asdf"){
        EXPECT_EQ(i, 1);
        EXPECT_STREQ(s, "asdf");
    };

    static std::string static_method(){return "static_method";}

    CopyableWrappedClass copyable_wrapped_class;
    std::unique_ptr<WrappedClass> up_wrapped_class;
};


class WrappedString : public WrappedClassBase {
public:
    std::string string;
    WrappedString(std::string string) : string(string) {}
};

namespace v8toolkit {
CAST_TO_NATIVE(WrappedString, {
    return WrappedString(CastToNative<std::string const &>()(isolate, value));
});

}


class WrappedClassFixture : public JavaScriptFixture {
public:
    WrappedClassFixture() {
        ISOLATE_SCOPED_RUN(*i);
        {
            auto & w = V8ClassWrapper<WrappedClass>::get_instance(*i);
            w.add_member<&WrappedClass::i>("i");
            w.add_member<&WrappedClass::ci>("ci");
            w.add_member<&WrappedClass::upf>("upf");
            w.add_member<&WrappedClass::cupf>("cupf");
            w.add_member<&WrappedClass::string>("string");
            w.add_member<&WrappedClass::copyable_wrapped_class>("copyable_wrapped_class");
            w.add_member<&WrappedClass::up_wrapped_class>("up_wrapped_class");
            w.add_method("takes_int_5", &WrappedClass::takes_int_5);
            w.add_method("takes_const_int_6", &WrappedClass::takes_const_int_6);
            w.add_method("default_parameters", &WrappedClass::default_parameters, std::tuple<int, char const *>(1, "asdf"));
            w.add_static_method("static_method", &WrappedClass::static_method);
            w.finalize();
            w.add_constructor("WrappedClass", *i);
        }
        {
            auto & w = V8ClassWrapper<CopyableWrappedClass>::get_instance(*i);
            w.finalize();
            w.add_constructor<>("CopyableWrappedClass", *i);
        }

        {
            auto & w = V8ClassWrapper<WrappedString>::get_instance(*i);
            w.add_member<&WrappedString::string>("string");
            w.finalize();
            w.add_constructor<std::string const &>("WrappedString", *i);
        }



        i->add_function("takes_wrapped_class_lvalue", [](WrappedClass & wrapped_class){
            EXPECT_EQ(wrapped_class.string, "string value");
            return wrapped_class.i;
        });

        i->add_function("takes_wrapped_class_rvalue", [](WrappedClass && wrapped_class){
            EXPECT_EQ(wrapped_class.string, "string value");
            WrappedClass wc(std::move(wrapped_class));
            EXPECT_EQ(wc.string, "string value");
            EXPECT_EQ(wrapped_class.string, "");

            return wrapped_class.i;
        });

        i->add_function("takes_wrapped_class_unique_ptr", [](std::unique_ptr<WrappedClass> wrapped_class){
            return wrapped_class->i;
        });

        i->add_function("returns_wrapped_class_lvalue_ref", []()->WrappedClass&{
            static WrappedClass static_wrapped_class;
            return static_wrapped_class;
        });

        create_context();

    }
};



TEST_F(WrappedClassFixture, Accessors) {


    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass().i == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().ci == 5)");

            c->run("EXPECT_TRUE(new WrappedClass().upf == 3.5)");
            c->run("EXPECT_TRUE(new WrappedClass().cupf == 4.5)");
        }
    });
}

TEST_F(WrappedClassFixture, SimpleFunctions) {

    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass().takes_int_5(5) == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().takes_const_int_6(6) == 6)");

            c->run("EXPECT_TRUE(WrappedClass.static_method() == `static_method`)");
        }
    });
}



TEST_F(WrappedClassFixture, CallingWithLvalueWrappedClass) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run(
                "wc = new WrappedClass(); takes_wrapped_class_lvalue(wc);"
                    "EXPECT_EQJS(wc.string, `string value`); wc;"
            );
            EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));
        }
        {

            // calling with non-owning object
            c->run("non_owning_wc = returns_wrapped_class_lvalue_ref();  takes_wrapped_class_lvalue(non_owning_wc);"
                       "EXPECT_EQJS(non_owning_wc.string, `string value`);"
            );

        }
    });
}

TEST_F(WrappedClassFixture, CallingWithRvalueWrappedClass) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run("wc = new WrappedClass(); takes_wrapped_class_rvalue(wc);"
                                     "EXPECT_EQJS(wc.string, ``); wc;"
            );

            // object still owns its memory, even though the contents may have been moved out of
            EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));
        }

        {
            // calling with non-owning object
            EXPECT_THROW(
                c->run("non_owning_wc = returns_wrapped_class_lvalue_ref(); takes_wrapped_class_rvalue(non_owning_wc);"
                           "EXPECT_EQJS(wc.string, ``);"
                ), V8Exception);
        }
    });
}



TEST_F(WrappedClassFixture, CallingWithUniquePtr) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run(
                "wc = new WrappedClass(); takes_wrapped_class_unique_ptr(wc);"
                    // "EXPECT_EQJS(wc.string, ``);" <== can't do this, the memory is *GONE* not just moved out of
                "wc;"
            );
            EXPECT_FALSE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));

        }
        {
            // call with unique_ptr when owning, then call again after first call takes ownership
            EXPECT_THROW(
                c->run(
                    "wc = new WrappedClass(); takes_wrapped_class_unique_ptr(wc); takes_wrapped_class_unique_ptr(wc);"
                        "EXPECT_EQJS(wc.string, ``); wc;"
                ), V8Exception);
        }
        {
            // calling with non-owning object
            EXPECT_THROW(
                c->run(
                    "non_owning_wc = returns_wrapped_class_lvalue_ref(); takes_wrapped_class_unique_ptr(non_owning_wc);"

                ), V8Exception);
        }
    });
}


TEST_F(WrappedClassFixture, ReturningUniquePtr) {

    c->add_function("returns_unique_ptr", [&](){
        return std::make_unique<WrappedClass>();
    });


    (*c)([&](){

        c->run("returns_unique_ptr();");

    });
}


TEST_F(WrappedClassFixture, FunctionTakesCopyOfWrappedType) {
    c->add_function("wants_copy_of_wrapped_type", [&](CopyableWrappedClass){});

    (*c)([&](){
        c->run("wants_copy_of_wrapped_type(new CopyableWrappedClass());");
    });

    // cannot make copy of uncopyable type
    if constexpr(CastToNative<WrappedClass>::callable()) {
        EXPECT_TRUE(false);
    }
}



// If the wrapped type has a custom CastToNative, use that instead of blindly trying to get the
//   C++ object pointer out of a JavaScript value at that position
TEST_F(WrappedClassFixture, PreferSpecializedCastToNativeDuringParameterBuilder) {
    (*c)([&]() {

        c->add_function("wants_wrapped_string_rvalue_ref", [&](WrappedString && wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "rvalue");
        });
        c->add_function("wants_wrapped_string", [&](WrappedString && wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "copy");
        });
        c->add_function("wants_wrapped_string_lvalue_ref", [&](WrappedString && wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "lvalue");

        });

        c->run("wants_wrapped_string_rvalue_ref(`rvalue`);");
        c->run("wants_wrapped_string_rvalue_ref(new WrappedString(`rvalue`));");

        c->run("wants_wrapped_string(`copy`);");
        c->run("wants_wrapped_string(new WrappedString(`copy`));");

        c->run("wants_wrapped_string_lvalue_ref(`lvalue`);");
        c->run("wants_wrapped_string_lvalue_ref(new WrappedString(`lvalue`));");
    });
}
//
//
//TEST_F(WrappedClassFixture, DefaultParameters) {
//
//    (*c)([&](){
//
//        c->run("new WrappedClass().default_parameters();");
//
//    });
//
//
//}
//



