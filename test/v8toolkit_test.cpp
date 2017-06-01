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
