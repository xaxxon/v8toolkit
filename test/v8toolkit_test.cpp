#include "testing.h"


TEST_F(JavaScriptFixture, RequireErrors) {

    this->create_context();

    (*c)([&]{
        v8::Local<v8::Value> value;
        EXPECT_FALSE(require(*c, "does_not_exist.js", value, {"."}));
        EXPECT_THROW(require(*c, "syntax_error.js", value, {"."}), V8CompilationException);
        EXPECT_THROW(require(*c, "runtime_error.js", value, {"."}), V8ExecutionException);
    });
}



TEST_F(JavaScriptFixture, ReleaseRequiredModulesBeforeIsolateGoesAway) {

    this->create_context();

    (*c)([&]{
        v8::Local<v8::Value> value;
        require(*c, "valid_module.js", value, {"."});
    });

    // get the Isolate destructor to call in the body of the test instead of implicitly in fixture destructor code
    c.reset();
    i.reset();

    // NO CODE AFTER THIS
}


