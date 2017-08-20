

#include <v8_class_wrapper.h>
#include <javascript.h>
#include "wrapped_class_base.h"
#include "testing.h"



void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
{
    return malloc(size);
}


using namespace v8toolkit;
using std::unique_ptr;
using std::make_unique;


class CopyableWrappedClass : public WrappedClassBase {
public:
    CopyableWrappedClass(){}
    CopyableWrappedClass(CopyableWrappedClass const &) = default;

};

bool takes_holder_called = false;
bool takes_this_called = false;
bool takes_isolate_and_int_called = false;
bool returns_wrapped_class_lvalue_called = false;
class WrappedClass : public WrappedClassBase {
public:
    // class is not default constructible
    WrappedClass(int i) : constructor_i(i) {};
    WrappedClass(WrappedClass const &) = delete;
    WrappedClass(WrappedClass &&) = default;
    virtual ~WrappedClass(){}

    int constructor_i;
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

    WrappedClass returns_uncopyable_type_by_value() {
        return WrappedClass{4};
    }

    bool default_parameters_called = false;
    void default_parameters(int j = 1,
                            char const * s = "asdf",
                            vector<std::string> && = {},
                            CopyableWrappedClass = {},
                            CopyableWrappedClass && = {},
                            CopyableWrappedClass * = nullptr){
        EXPECT_EQ(j, 1);
        EXPECT_STREQ(s, "asdf");
        this->default_parameters_called = true;
    };

    WrappedClass & returns_wrapped_class_lvalue() {
        returns_wrapped_class_lvalue_called = true;
        return *this;
    }

    static std::string static_method(int i = 5, char const * str = "asdf"){
        EXPECT_EQ(i, 5);
        EXPECT_STREQ(str, "asdf");
        return "static_method";}

    CopyableWrappedClass copyable_wrapped_class;
    std::unique_ptr<WrappedClass> up_wrapped_class;

    void takes_const_wrapped_ref(WrappedClass const &) {}
    bool takes_const_unwrapped_ref(std::string_view const & name) { return false; }


    static void takes_isolate_and_int(v8::Isolate * isolate, int i, v8::Local<v8::Value> value, int j) {
        EXPECT_EQ(i, 1);
        EXPECT_EQ(v8toolkit::CastToNative<int>()(isolate, value), 2);
        EXPECT_EQ(j, 3);
        takes_isolate_and_int_called = true;
    }

    void takes_this(v8::Isolate * isolate, v8toolkit::This this_object) {
        EXPECT_EQ(V8ClassWrapper<WrappedClass>::get_instance(isolate).get_cpp_object(this_object), nullptr);
        takes_this_called = true;
    }
};

class WrappedClassChild : public WrappedClass {
public:
    WrappedClassChild() : WrappedClass(0) {}
};


class WrappedString : public WrappedClassBase {
public:
    std::string string;
    WrappedString(std::string string) : string(string) {}
};

namespace v8toolkit {
CAST_TO_NATIVE(WrappedString, {
    return WrappedString(CastToNative<std::string>()(isolate, value));
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
            w.add_method("takes_const_wrapped_ref", &WrappedClass::takes_const_wrapped_ref);
            w.add_static_method("takes_isolate_and_int", &WrappedClass::takes_isolate_and_int, std::tuple<int>(3));
            w.add_method("takes_this", &WrappedClass::takes_this);
            w.add_method("returns_wrapped_class_lvalue", &WrappedClass::returns_wrapped_class_lvalue);
            w.add_method("takes_const_unwrapped_ref", &WrappedClass::takes_const_unwrapped_ref);
            w.add_method("returns_uncopyable_type_by_value", &WrappedClass::returns_uncopyable_type_by_value);
            w.add_method("default_parameters", &WrappedClass::default_parameters,
                         std::tuple<
                             int,
                             char const *,
                             vector<std::string>,
                             CopyableWrappedClass,
                             CopyableWrappedClass,
                             CopyableWrappedClass*>(1, "asdf", {}, {}, {}, nullptr));
            w.add_static_method("static_method", &WrappedClass::static_method, std::make_tuple(5, "asdf"));
            w.add_static_method("inline_static_method", [](int i){
                EXPECT_EQ(i, 7);
            }, std::tuple<int>(7));
            w.add_enum("enum_test", {{"A", 1}, {"B", 2}, {"C", 3}});
            w.set_compatible_types<WrappedClassChild>();
            w.finalize(true);
            w.add_constructor<int>("WrappedClass", *i);
        }
        {
            auto & w = V8ClassWrapper<WrappedClassChild>::get_instance(*i);
            w.set_parent_type<WrappedClass>();
            w.finalize(true);
            w.add_constructor("WrappedClassChild", *i);
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

            static WrappedClass static_wrapped_class(1);
            return static_wrapped_class;
        });

        create_context();

    }
};



TEST_F(WrappedClassFixture, Accessors) {


    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(1).i == 5)");
            c->run("EXPECT_TRUE(new WrappedClass(2).ci == 5)");

            c->run("EXPECT_TRUE(new WrappedClass(3).upf == 3.5)");
            c->run("EXPECT_TRUE(new WrappedClass(4).cupf == 4.5)");
        }
    });
}

TEST_F(WrappedClassFixture, SimpleFunctions) {

    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(5).takes_int_5(5) == 5)");
            c->run("EXPECT_TRUE(new WrappedClass(6).takes_const_int_6(6) == 6)");

            c->run("EXPECT_TRUE(WrappedClass.static_method() == `static_method`)");
        }
    });
}

TEST_F(WrappedClassFixture, Enumerations) {
    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.A == 1)");
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.B == 2)");
            c->run("EXPECT_TRUE(new WrappedClass(5).enum_test.C == 3)");
        }
    });

}



TEST_F(WrappedClassFixture, CallingWithLvalueWrappedClass) {

    (*c)([&] {
        {
            // calling with owning object
            auto result = c->run(
                "wc = new WrappedClass(7); takes_wrapped_class_lvalue(wc);"
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
            auto result = c->run("wc = new WrappedClass(8); takes_wrapped_class_rvalue(wc);"
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
                "wc = new WrappedClass(9); takes_wrapped_class_unique_ptr(wc);"
                    // "EXPECT_EQJS(wc.string, ``);" <== can't do this, the memory is *GONE* not just moved out of
                "wc;"
            );
            EXPECT_FALSE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result.Get(*i)->ToObject()));

        }
        {
            // call with unique_ptr when owning, then call again after first call takes ownership
            EXPECT_THROW(
                c->run(
                    "wc = new WrappedClass(10); takes_wrapped_class_unique_ptr(wc); takes_wrapped_class_unique_ptr(wc);"
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
        return std::make_unique<WrappedClass>(4);
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
    // cannot make copy of uncopyable type
    bool copy_of_copyable_type = false;
    if constexpr(CastToNative<CopyableWrappedClass>::callable()) {
        copy_of_copyable_type = true;
    }
    EXPECT_TRUE(copy_of_copyable_type);
}



// If the wrapped type has a custom CastToNative, use that instead of blindly trying to get the
//   C++ object pointer out of a JavaScript value at that position
TEST_F(WrappedClassFixture, PreferSpecializedCastToNativeDuringParameterBuilder) {
    (*c)([&]() {

        c->add_function("wants_wrapped_string_rvalue_ref", [&](WrappedString && wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "rvalue");
        });
        c->add_function("wants_wrapped_string", [&](WrappedString wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "copy");
        });
        c->add_function("wants_wrapped_string_lvalue_ref", [&](WrappedString & wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "lvalue");
        });
        c->add_function("wants_wrapped_string_const_lvalue_ref", [&](WrappedString const & wrapped_string) {
            EXPECT_EQ(wrapped_string.string, "lvalue");
        });


        c->run("wants_wrapped_string_rvalue_ref(`rvalue`);");
        c->run("wants_wrapped_string_rvalue_ref(new WrappedString(`rvalue`));");

        c->run("wants_wrapped_string(`copy`);");
        c->run("wants_wrapped_string(new WrappedString(`copy`));");

        c->run("wants_wrapped_string_lvalue_ref(`lvalue`);");
        c->run("wants_wrapped_string_lvalue_ref(new WrappedString(`lvalue`));");

        c->run("wants_wrapped_string_const_lvalue_ref(`lvalue`);");
        c->run("wants_wrapped_string_const_lvalue_ref(new WrappedString(`lvalue`));");
    });
}


TEST_F(WrappedClassFixture, DefaultParameters) {

    (*c)([&](){

        auto result = c->run("let wc = new WrappedClass(11); wc.default_parameters(); wc;");
        WrappedClass * pwc = CastToNative<WrappedClass *>()(*i, result.Get(*i));
        EXPECT_TRUE(pwc->default_parameters_called);
    });

}



TEST_F(WrappedClassFixture, DerivedTypesLValue) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &{
        static WrappedClassChild wcc;
        return wcc;
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}



TEST_F(WrappedClassFixture, DerivedTypesPointer) {
    c->add_function("wants_wrapped_class", [&](WrappedClass *) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild *{
        static WrappedClassChild wcc;
        return &wcc;
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}

TEST_F(WrappedClassFixture, DerivedTypesRValue) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &&) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &&{
        static WrappedClassChild wcc;
        return std::move(wcc);
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");

}


TEST_F(WrappedClassFixture, DerivedTypesLValueRValueMismatch) {
    c->add_function("wants_wrapped_class", [&](WrappedClass &&) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->WrappedClassChild &{
        static WrappedClassChild wcc;
        return wcc;
    });

    EXPECT_THROW(
        c->run("wants_wrapped_class(returns_wrapped_class_child());"),
        V8Exception
    );
}

TEST_F(WrappedClassFixture, DerivedTypesUniquePointer) {
    c->add_function("wants_wrapped_class", [&](std::unique_ptr<WrappedClass>) {
    });
    c->add_function("returns_wrapped_class_child", [&]()->std::unique_ptr<WrappedClassChild> {
        return std::make_unique<WrappedClassChild>();
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");
}


TEST_F(WrappedClassFixture, DerivedTypesUniquePointerReverseCast) {
    c->add_function("wants_wrapped_class", [&](std::unique_ptr<WrappedClassChild>) {
    });

    // really return a wrapped class child, but call it a wrapped class instead
    c->add_function("returns_wrapped_class_child", [&]()->std::unique_ptr<WrappedClass> {
        return std::make_unique<WrappedClass>(6);
    });

    c->run("wants_wrapped_class(returns_wrapped_class_child());");
}


TEST_F(WrappedClassFixture, CastToJSRValueRef) {
    WrappedClass wc(2);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}


TEST_F(WrappedClassFixture, TakesConstWrappedRef) {
    WrappedClass wc(3);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}




TEST_F(WrappedClassFixture, TakesConstUnwrappedRef) {
    WrappedClass wc(4);
    (*c)([&]() {

        auto result = CastToJS<WrappedClass &&>()(*i, std::move(wc));
        EXPECT_TRUE(V8ClassWrapper<WrappedClass>::does_object_own_memory(result->ToObject()));
    });
}


// test calling
TEST_F(WrappedClassFixture, StaticMethodDefaultValue) {
    c->run("WrappedClass.static_method(5)");
    c->run("WrappedClass.static_method()");
    c->run("WrappedClass.static_method()");

    c->run("WrappedClass.inline_static_method(7);");
    c->run("WrappedClass.inline_static_method();");
    c->run("WrappedClass.inline_static_method();");
}


TEST_F(WrappedClassFixture, FunctionTakesIsolatePointer) {
    takes_isolate_and_int_called = false;
    c->run("WrappedClass.takes_isolate_and_int(1, 2, 3);");
    EXPECT_TRUE(takes_isolate_and_int_called);

    takes_isolate_and_int_called = false;
    c->run("WrappedClass.takes_isolate_and_int(1, 2);");
    EXPECT_TRUE(takes_isolate_and_int_called);

}


TEST_F(WrappedClassFixture, WrapDerivedTypeFromBaseWrapper) {
    (*c)([&]() {

        returns_wrapped_class_lvalue_called = false;
        auto result = c->run("new WrappedClass(12).returns_wrapped_class_lvalue();");
        EXPECT_TRUE(returns_wrapped_class_lvalue_called);
        auto wrapped_class = get_value_as<WrappedClass*>(*i, result);
        EXPECT_EQ(wrapped_class->i, 5);
    });
}


TEST_F(WrappedClassFixture, FunctionTakesHolder) {
    (*c)([&]() {

        takes_this_called = false;
        c->run("var wc2 = new WrappedClass(13);wc2.base = true;"
                   "var derived_wc2 = Object.create(wc2); derived_wc2.derived = true;");

        auto base = c->run("wc2");
        auto derived = c->run("derived_wc2");



        c->run("derived_wc2.takes_this();");
        EXPECT_TRUE(takes_this_called);
    });
}




TEST_F(WrappedClassFixture, CastToNativeNonCopyableTypeByValue) {
    auto isolate = c->isolate;
    (*c)([&]() {
        auto wrapped_class = c->run("new WrappedClass(40);");
        EXPECT_EQ(CastToNative<WrappedClass>()(isolate, wrapped_class.Get(isolate)).constructor_i, 40);

        // try again, but with a non-owning javascript object
        auto wrapped_class2 = c->run("new WrappedClass(41);");
        auto & wrapper = v8toolkit::V8ClassWrapper<WrappedClass>::get_instance(isolate);
        wrapper.release_internal_field_memory(wrapped_class2.Get(isolate)->ToObject());
        EXPECT_THROW(CastToNative<WrappedClass>()(c->isolate, wrapped_class2.Get(isolate)), CastException);
    });
}


