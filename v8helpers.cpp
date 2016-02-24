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

void set_global_object_alias(v8::Isolate * isolate, const v8::Local<v8::Context> context, std::string alias_name)
{
    auto global_object = context->Global();
    (void)global_object->Set(context, v8::String::NewFromUtf8(isolate, alias_name.c_str()), global_object);
    
}



}