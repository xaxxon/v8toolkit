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



// void MyClass(const v8::FunctionCallbackInfo<v8::Value> & info) {
//   printf("Allocating MyClass object\n");
//   auto js_object = info.This();
//   int * i = new int();
//   printf("Allocating int at %p\n", i);


//   js_object->SetInternalField(0, v8::External::New(info.GetIsolate(),i));

// }

// void do_something(const v8::FunctionCallbackInfo<v8::Value> & info) {
//   printf("Top of do_something\n");
//   Global<FunctionTemplate> * gftp = (Global<FunctionTemplate>*)Local<External>::Cast(info.Data())->Value();
//   printf("Got gftp at %p\n", gftp);
//   auto instance = info.Holder()->FindInstanceInPrototypeChain(gftp->Get(info.GetIsolate()));
//   if (instance.IsEmpty()) return; 
//   void* ptr = instance->GetAlignedPointerFromInternalField(0); 


// //   auto self = info.Holder();
// //   auto ptr =  v8::Local<v8::External>::Cast(self->GetInternalField(0))->Value();
// //   while(ptr==nullptr) {
// //     printf("Nope, checking prototype\n");
// //     self=Local<Object>::Cast(self->GetPrototype());
// //     ptr =  v8::Local<v8::External>::Cast(self->GetInternalField(0))->Value();
// //   }
//   printf("Eventually got internal field pointer of %p\n", ptr);
// }

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
		auto got = v8::ObjectTemplate::New(i);

		//auto ft = v8::FunctionTemplate::New(i);
		//	ft->SetClassName(v8::String::NewFromUtf8(i, "MyClass"));
		//ft->InstanceTemplate()->SetInternalFieldCount(1);
		//got->Set(i, "MyClass", ft);

		//	auto ft2 = v8::FunctionTemplate::New(i);
		//auto gftp = new Global<FunctionTemplate>(i,ft);
		//printf("Created gftp at %p\n", gftp);
		//	ft2->SetCallHandler(do_something, v8::External::New(i,gftp));
		//	ft->InstanceTemplate()->Set(i, "do_something", ft2);

		auto security_token = String::NewFromUtf8(i,"SecTok");


		auto c = v8::Context::New(i, nullptr, got);
		c->SetSecurityToken(security_token);
		{
		  v8::Context::Scope cs(c);


		  //	auto s = v8::Script::Compile(c, v8::String::NewFromUtf8(i,"a=new MyClass();a.do_something(); b=Object.create(a); b.do_something();a.do_something();")).ToLocalChecked();

		 auto s = v8::Script::Compile(c, v8::String::NewFromUtf8(i,"a=function(){return b;}")).ToLocalChecked();

		  (void)s->Run(c);
		}


		(void)c->Global()->Set(c,String::NewFromUtf8(i,"b"), String::NewFromUtf8(i,"HELLO1"));
		//auto a_val = c->Global()->Get(c, String::NewFromUtf8(i,"a")).ToLocalChecked();
		//auto a_func = Local<Function>::Cast(a_val);
		//auto result = a_func->Call(c, c->Global(), 0, nullptr);
		//printf("result: %s\n", *String::Utf8Value(result.ToLocalChecked()));



		auto c2 = v8::Context::New(i, nullptr, got);
		c2->SetSecurityToken(security_token);
		{
		  Context::Scope cs2(c2);
		  //(void)c2->Global()->Set(c2,String::NewFromUtf8(i,"b"), String::NewFromUtf8(i,"HELLO2"));
		  auto a_val2 = c->Global()->Get(c, String::NewFromUtf8(i,"a")).ToLocalChecked();
		  auto a_func2 = Local<Function>::Cast(a_val2);
		  auto result2 = a_func2->Call(c2, c2->Global(), 0, nullptr);
		  printf("result: %s\n", *String::Utf8Value(result2.ToLocalChecked()));
		}
	}
}
