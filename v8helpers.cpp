#include <assert.h>

#include "v8helpers.h"

namespace v8toolkit {

int get_array_length(v8::Isolate * isolate, v8::Local<v8::Array> array) {
    auto context = isolate->GetCurrentContext();
    return array->Get(context, v8::String::NewFromUtf8(isolate, "length")).ToLocalChecked()->Uint32Value(); 
}



int get_array_length(v8::Isolate * isolate, v8::Local<v8::Value> array_value)
{
    if(array_value->IsArray()) {
        return get_array_length(isolate, v8::Local<v8::Array>::Cast(array_value));
    } else {
        // TODO: probably throw?
        assert(array_value->IsArray());
    }
    assert(false); // shut up the compiler
}


}