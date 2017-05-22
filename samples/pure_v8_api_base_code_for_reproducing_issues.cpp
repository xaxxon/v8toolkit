#include <string.h>
#include <stdlib.h>

#include "libplatform/libplatform.h"
#include "v8.h"

using namespace v8;

class ArrayBufferAllocator : public v8::ArrayBuffer::Allocator {
 public:
  inline virtual void* Allocate(size_t length) override {
    void* data = AllocateUninitialized(length);
    return data == NULL ? data : memset(data, 0, length);
  }
  inline virtual void* AllocateUninitialized(size_t length) override { return malloc(length); }
  inline virtual void Free(void* data, size_t) override { free(data); }
};



void add_handler_to_object_template(Local<ObjectTemplate> ot) {

    ot->SetHandler(v8::NamedPropertyHandlerConfiguration(
            // Getter
            [](v8::Local<v8::Name> property_name,
               v8::PropertyCallbackInfo<v8::Value> const & info){
                printf("IN GETTER CALLBACK1112 %s\n", *v8::String::Utf8Value(property_name));

                info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), "this value created by getter"));
            },
            // setter
            [](v8::Local<v8::Name> property_name,
               v8::Local<v8::Value> new_property_value,
               v8::PropertyCallbackInfo<v8::Value> const & info){
                printf("IN SETTER CALLBACK222 %s\n", *v8::String::Utf8Value(property_name));
//
                info.GetReturnValue().Set(true);
            },
            // query - returns attributes on the given property name
            // http://brendanashworth.github.io/v8-docs/namespacev8.html#a05f25f935e108a1ea2d150e274602b87
            [](v8::Local< v8::Name > property_name, v8::PropertyCallbackInfo< v8::Integer> const & info){
                printf("In query callback %s\n", *v8::String::Utf8Value(property_name));
                info.GetReturnValue().Set(v8::None);
            },
            // deleter
            [](v8::Local<v8::Name> property_name,
               v8::PropertyCallbackInfo<v8::Boolean> const & info){
                printf("IN DELETER CALLBACK333 %s\n", *v8::String::Utf8Value(property_name));
                info.GetReturnValue().Set(true);
            },
            // enumerator
            [](v8::PropertyCallbackInfo<v8::Array> const & info) {
                printf("IN ENUMERATOR CALLBACK444\n");

                auto array = v8::Array::New(info.GetIsolate(), 1);
                array->Set(0, v8::String::NewFromUtf8(info.GetIsolate(), "foo"));
            }
    ));

}


Local<ObjectTemplate> make_object_with_named_property_callbacks(Isolate * i) {

    auto ft = FunctionTemplate::New(i);
    ft->SetClassName(String::NewFromUtf8(i, "TestObject"));
    auto parent_ft = FunctionTemplate::New(i);
    parent_ft->InstanceTemplate()->Set(i, "parent_foo", ObjectTemplate::New(i));
    parent_ft->InstanceTemplate()->SetAccessor(v8::String::NewFromUtf8(i, "parent accessor"), [](Local<String> property,
                                                             const PropertyCallbackInfo<Value>& info){info.GetReturnValue().Set(v8::String::NewFromUtf8(info.GetIsolate(), "value from accessor getter"));});
    add_handler_to_object_template(parent_ft->InstanceTemplate());
    parent_ft->PrototypeTemplate()->Set(i, "parent_bar", FunctionTemplate::New(i, [](const FunctionCallbackInfo< Value > &info){info.GetReturnValue().Set(444);}));
    parent_ft->PrototypeTemplate()->Set(i, "parent_obj", ObjectTemplate::New(i));
    ft->Inherit(parent_ft);

    v8::Local<v8::ObjectTemplate> instance_template = ft->InstanceTemplate();
    instance_template->SetInternalFieldCount(1);
    add_handler_to_object_template(instance_template);
    instance_template->SetAccessor(v8::String::NewFromUtf8(i, "accessor"), [](Local<String> property,
              const PropertyCallbackInfo<Value>& info){info.GetReturnValue().Set(v8::String::NewFromUtf8(info.GetIsolate(), "value from accessor getter"));});

    auto prototype_template = ft->PrototypeTemplate();
    prototype_template->Set(i, "foo", FunctionTemplate::New(i, [](const FunctionCallbackInfo< Value > &info){info.GetReturnValue().Set(44);}));
    prototype_template->Set(i, "bar", ObjectTemplate::New(i));

    auto context_template = ObjectTemplate::New(i);
    context_template->Set(String::NewFromUtf8(i, "Test"), ft);
    return context_template;
}


int main(int argc, char ** argv)
{
	v8::V8::InitializeICU();
	auto platform = v8::platform::CreateDefaultPlatform();
	v8::V8::InitializePlatform(platform);
	v8::V8::Initialize();

	v8::Isolate::CreateParams create_params;
	create_params.array_buffer_allocator = (v8::ArrayBuffer::Allocator *) new ArrayBufferAllocator();

	auto i = v8::Isolate::New(create_params);

	{
		v8::Isolate::Scope is(i);
		v8::HandleScope hs(i);

		auto c = v8::Context::New(i, nullptr,  make_object_with_named_property_callbacks(i));
        {
		  v8::Context::Scope cs(c);
//            auto callback_object = make_object_with_named_property_callbacks(c);
//
//            c->Global()->Set(v8::String::NewFromUtf8(i, "object_with_callbacks"), callback_object);


		 auto s = v8::Script::Compile(c, v8::String::NewFromUtf8(i,"obj = new Test(); obj.accessorxxx=4")).ToLocalChecked();

		  printf("result: %s\n", *String::Utf8Value(s->Run(c).ToLocalChecked()));
		}


	}
}
