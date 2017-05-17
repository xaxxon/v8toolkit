
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

    int takes_int_5(int x) {
        EXPECT_EQ(x, 5);
        return x;
    }
    int takes_const_int_6(int const x) {
        EXPECT_EQ(x, 6);
        return x;
    }


};


TEST_F(JavaScriptFixture, WrappedType) {

    ISOLATE_SCOPED_RUN(*i);
    auto & w = V8ClassWrapper<WrappedClass>::get_instance(*i);
    w.add_member<&WrappedClass::i>("i");
    w.add_member<&WrappedClass::ci>("ci");
    w.add_member<&WrappedClass::upf>("upf");
    w.add_member<&WrappedClass::cupf>("cupf");
    w.add_method("takes_int_5", &WrappedClass::takes_int_5);
    w.add_method("takes_const_int_6", &WrappedClass::takes_const_int_6);
    w.finalize();
    w.add_constructor("WrappedClass", *i);

    create_context();

    (*c)([&] {
        {
            c->run("EXPECT_TRUE(new WrappedClass().i == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().ci == 5)");

            c->run("EXPECT_TRUE(new WrappedClass().upf == 3.5)");
            c->run("EXPECT_TRUE(new WrappedClass().cupf == 4.5)");

            c->run("EXPECT_TRUE(new WrappedClass().takes_int_5(5) == 5)");
            c->run("EXPECT_TRUE(new WrappedClass().takes_const_int_6(6) == 6)");


        }
    });

}