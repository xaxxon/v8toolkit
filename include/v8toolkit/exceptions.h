#pragma once

#include <v8.h>

#include "log.h"

namespace v8toolkit {

/**
* When the V8 engine itself generates an error (or a user calls isolate->ThrowException manually with a v8::Value for some reason)
* That exception is re-thrown as a standard C++ exception of this type.   The V8 Value thrown is available.
* get_local_value must be called within a HandleScope
* get_value returns a new Global handle to the value.
*/
class V8Exception : public std::exception {
private:
    v8::Isolate * isolate;
    v8::Global<v8::Value> value;
    std::string value_for_what;

public:
    V8Exception(v8::Isolate * isolate, v8::Global<v8::Value>&& value) : isolate(isolate), value(std::move(value)) {
        std::string str(*v8::String::Utf8Value(this->value.Get(isolate)));
        value_for_what = str == "" ? "unknown error" : str;
        log.error(LogT::Subjects::RUNTIME_EXCEPTION, "V8Exception: {}", this->value_for_what);
    }
    V8Exception(v8::Isolate * isolate, v8::Local<v8::Value> value) : V8Exception(isolate, v8::Global<v8::Value>(isolate, value)) {}
    V8Exception(v8::Isolate * isolate, std::string reason) : V8Exception(isolate, v8::String::NewFromUtf8(isolate, reason.c_str())) {}
    virtual const char * what() const noexcept override {
        return value_for_what.c_str();
    }
    v8::Local<v8::Value> get_local_value(){return value.Get(isolate);}
    v8::Isolate * get_isolate(){return isolate;}
    v8::Global<v8::Value> get_value(){return v8::Global<v8::Value>(isolate, value);}
};


class V8AssertionException : public V8Exception {
public:
    V8AssertionException(v8::Isolate * isolate, v8::Local<v8::Value> value) :
        V8Exception(isolate, value)
    {}

    V8AssertionException(v8::Isolate * isolate, v8::Global<v8::Value>&& value) :
        V8Exception(isolate, std::forward<v8::Global<v8::Value>>(value))
    {}

    V8AssertionException(v8::Isolate * isolate, std::string reason) :
        V8Exception(isolate, reason)
    {}
};


class V8ExecutionException : public V8Exception {
    std::string stacktrace;
public:

    V8ExecutionException(v8::Isolate * isolate, v8::TryCatch & tc) :
        V8Exception(isolate, tc.Exception())
    {
        auto stacktrace_maybe = tc.StackTrace(isolate->GetCurrentContext());
        if (!stacktrace_maybe.IsEmpty()) {
            stacktrace = *v8::String::Utf8Value(stacktrace_maybe.ToLocalChecked());
        } else {
            stacktrace = "No stacktrace available";
        }
        log.error(LogT::Subjects::RUNTIME_EXCEPTION, "V8ExecutionException: {}", this->stacktrace);
    }
    const std::string & get_stacktrace(){return stacktrace;}
};


/**
* Same as a V8 exception, except if this type is thrown it indicates the exception was generated
*   during compilation, not at runtime.
*/
class V8CompilationException : public V8ExecutionException {
public:
    using V8ExecutionException::V8ExecutionException;
};


}