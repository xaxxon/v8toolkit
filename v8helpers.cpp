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



void print_v8_value_details(v8::Local<v8::Value> local_value) {
    
    auto value = *local_value;
    
    std::cout << "undefined: " << value->IsUndefined() << std::endl;
    std::cout << "null: " << value->IsNull() << std::endl;
    std::cout << "true: " << value->IsTrue() << std::endl;
    std::cout << "false: " << value->IsFalse() << std::endl;
    std::cout << "name: " << value->IsName() << std::endl;
    std::cout << "string: " << value->IsString() << std::endl;
    std::cout << "symbol: " << value->IsSymbol() << std::endl;
    std::cout << "function: " << value->IsFunction() << std::endl;
    std::cout << "array: " << value->IsArray() << std::endl;
    std::cout << "object: " << value->IsObject() << std::endl;
    std::cout << "boolean: " << value->IsBoolean() << std::endl;
    std::cout << "number: " << value->IsNumber() << std::endl;
    std::cout << "external: " << value->IsExternal() << std::endl;
    std::cout << "isint32: " << value->IsInt32() << std::endl;
    std::cout << "isuint32: " << value->IsUint32() << std::endl;
    std::cout << "date: " << value->IsDate() << std::endl;
    std::cout << "argument object: " << value->IsArgumentsObject() << std::endl;
    std::cout << "boolean object: " << value->IsBooleanObject() << std::endl;
    std::cout << "number object: " << value->IsNumberObject() << std::endl;
    std::cout << "string object: " << value->IsStringObject() << std::endl;
    std::cout << "symbol object: " << value->IsSymbolObject() << std::endl;
    std::cout << "native error: " << value->IsNativeError() << std::endl;
    std::cout << "regexp: " << value->IsRegExp() << std::endl;
    std::cout << "generator function: " << value->IsGeneratorFunction() << std::endl;
    std::cout << "generator object: " << value->IsGeneratorObject() << std::endl;
}


}