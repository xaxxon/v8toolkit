
#include <v8_class_wrapper.h>
#include <javascript.h>
#include "wrapped_class_base.h"
#include "testing.h"


using namespace v8toolkit;
using std::unique_ptr;
using std::make_unique;



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

    static std::string static_method(){return "static_method";}
};




TEST_F(JavaScriptFixture, WrappedType) {

    ISOLATE_SCOPED_RUN(*i);
    auto & w = V8ClassWrapper<WrappedClass>::get_instance(*i);
    w.add_member<&WrappedClass::i>("i");
    w.add_member<&WrappedClass::ci>("ci");
    w.add_member<&WrappedClass::upf>("upf");
    w.add_member<&WrappedClass::cupf>("cupf");
    w.add_member<&WrappedClass::string>("string");

    w.add_method("takes_int_5", &WrappedClass::takes_int_5);
    w.add_method("takes_const_int_6", &WrappedClass::takes_const_int_6);
    w.add_static_method("static_method", &WrappedClass::static_method);
    w.finalize();
    w.add_constructor("WrappedClass", *i);

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

    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass().i == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().ci == 5)");

            c->run("EXPECT_TRUE(new WrappedClass().upf == 3.5)");
            c->run("EXPECT_TRUE(new WrappedClass().cupf == 4.5)");

            c->run("EXPECT_TRUE(new WrappedClass().takes_int_5(5) == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().takes_const_int_6(6) == 6)");

            c->run("EXPECT_TRUE(WrappedClass.static_method() == `static_method`)");

            c->run("wc = new WrappedClass(); takes_wrapped_class_lvalue(wc);"
                       "EXPECT_EQJS(wc.string, `string value`);"
            );
            c->run("wc = new WrappedClass(); takes_wrapped_class_rvalue(wc);"
                       "EXPECT_EQJS(wc.string, ``);"
            );

            // cannot send a non-memory-owning wrapped type to a function taking an rvalue ref
            EXPECT_THROW(
            c->run("non_owning_wc = returns_wrapped_class_lvalue_ref(); takes_wrapped_class_rvalue(non_owning_wc);"
                       "EXPECT_EQJS(wc.string, ``);"
            ), V8Exception);

            c->run("wc = new WrappedClass(); takes_wrapped_class_unique_ptr(wc);"
                "EXPECT_EQJS(wc.string, ``);"
            );
        }
    });

}