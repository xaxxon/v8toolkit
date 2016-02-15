#include <vector>
#include <map>
#include <functional>

#include "javascript.h"

using namespace std;
using namespace v8toolkit;


namespace v8toolkit {


template<class T>
class JSWrapper {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Object> global_js_object;
    using BASE_TYPE = T;
public:
    JSWrapper(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : 
        isolate(context->GetIsolate()), 
        global_context(v8::Global<v8::Context>(isolate, context)),
        global_js_object(v8::Global<v8::Object>(isolate, object)) {}
};


// Builds the body of a JS_ACCESS function 
// Takes the return type of the function, the name of the function, and a list of input variable names, if any
#define JS_ACCESS_CORE(ReturnType, name, ...) \
    auto parameter_tuple = std::make_tuple( __VA_ARGS__ );\
    CastToNative<ReturnType> cast_to_native;\
    return scoped_run(isolate, global_context, [&](auto isolate, auto context){    \
        v8::TryCatch tc(isolate);                                                                             \
        auto js_object = global_js_object.Get(isolate);                                                       \
        auto maybe_function_value = js_object->Get(context, v8::String::NewFromUtf8(isolate, #name));    \
        if(!maybe_function_value.IsEmpty()) {  \
            if(!maybe_function_value.ToLocalChecked()->IsUndefined()) {                                       \
                auto jsfunction = v8::Local<v8::Function>::Cast(maybe_function_value.ToLocalChecked());       \
                if(!jsfunction.IsEmpty()) {   \
                    v8::Local<v8::Value> result = call_javascript_function(context, jsfunction, js_object, parameter_tuple);  \
                    return cast_to_native(isolate, result);\
                }\
            }\
        }\
        return this->std::remove_pointer<decltype(this)>::type::BASE_TYPE::name( __VA_ARGS__ );\
    });\


// defines a JS_ACCESS function for a method taking no parameters
#define JS_ACCESS(return_type, name)\
virtual return_type name() override {\
    JS_ACCESS_CORE(return_type, name)\
}

#define JS_ACCESS_1(return_type, name, t1)\
virtual return_type name(t1 p1) override {\
    JS_ACCESS_CORE(return_type, name, p1)\
}

#define JS_ACCESS_2(return_type, name, t1, t2)\
virtual return_type name(t1 p1, t2 p2) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2)\
}

#define JS_ACCESS_3(return_type, name, t1, t2, t3)\
virtual return_type name(t1 p1, t2 p2, t3 p3) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3)\
}

#define JS_ACCESS_4(return_type, name, t1, t2, t3, t4)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4)\
}

#define JS_ACCESS_5(return_type, name, t1, t2, t3, t4, t5)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5)\
}

#define JS_ACCESS_6(return_type, name, t1, t2, t3, t4, t5, t6)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6)\
}

#define JS_ACCESS_7(return_type, name, t1, t2, t3, t4, t5, t6, t7)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7)\
}

#define JS_ACCESS_8(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8)\
}

#define JS_ACCESS_9(return_type, name, t1, t2, t3, t4, t5, t6, t7, t8, t9)\
virtual return_type name(t1 p1, t2 p2, t3 p3, t4 p4, t5 p5, t6 p6, t7 p7, t8 p8, t9 p9) override {\
    JS_ACCESS_CORE(return_type, name, p1, p2, p3, p4, p5, p6, p7, p8, p9)\
}

}

   

class Animal {
public:
    string get_type() {return "Cow";}
    virtual string get_name() {return "This is the c++ get_name";};
    virtual string echo(string s){return s;}
    virtual int add(int i, int j){return i + j;}
};
    
class JSAnimal : public Animal, public JSWrapper<Animal> {
public:
    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : JSWrapper(context, object) {}
    JS_ACCESS(string, get_name)
    JS_ACCESS_1(string, echo, string)
    JS_ACCESS_2(int, add, int, int)
};


template<class T>
class Factory {
public:
    virtual T * operator()() = 0;
};


template<class T>
class JSFactory : public Factory<T> {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Function> global_javascript_function;
    
public:
    JSFactory(v8::Isolate * isolate, v8::Local<v8::Function> javascript_function) : 
        isolate(isolate),
        global_context(v8::Global<v8::Context>(isolate, isolate->GetCurrentContext())),
        global_javascript_function(v8::Global<v8::Function>(isolate, javascript_function))
        {
            printf("In constructor isolate set to %p\n", this->isolate);
            assert(this->isolate);
            assert(this->isolate->InContext());
        }
    
    T * operator()(){
        printf("In operator() with isolate %p\n", isolate);
        v8::Locker l(isolate);
        v8::HandleScope hs(isolate);

        auto context = global_context.Get(isolate);
        
        return scoped_run(isolate, context,[&](){
            printf("In JSFactory op() code\n");
            v8::TryCatch tc(isolate);
            printf("About to call function\n");
            if(tc.HasCaught()) {
                assert(false);
            }
            auto maybe_result = global_javascript_function.Get(isolate)->Call(context, context->Global(), 0, nullptr);
            printf("Back from function\n");
            if(tc.HasCaught()) {
                printf("%s\n", *v8::String::Utf8Value(tc.Exception()));
                assert(false);
            }
            auto result = maybe_result.ToLocalChecked();
            if(result->IsObject()){
                return new JSAnimal(context, v8::Local<v8::Object>::Cast(result));
            } else {
                assert(false);
            }
        });
    }
};



map<string, std::unique_ptr<Factory<Animal>>> animal_factories;
vector<Animal*> animals;


void register_animal_factory(v8::Isolate * isolate, string type, v8::Local<v8::Function> factory_method) {
    printf("In registere animal factory got isolate: %p\n", isolate);
    animal_factories.emplace(type, make_unique<JSFactory<Animal>>(isolate, factory_method));
}



int main(int argc, char ** argv)
{
    PlatformHelper::init(argc, argv);
       
    auto i = PlatformHelper::create_isolate();
    i->add_print();
    printf("Created isolate %p\n", i->get_isolate());
    auto & animal = i->wrap_class<Animal>();
    animal.add_method(&Animal::get_type, "get_type").add_method(&Animal::get_name, "get_name");
    
    i->add_function("add_animal", &register_animal_factory);
    
    auto c = i->create_context();
    
    c->run("add_animal('mule', function(){println('Returning new mule');return Object.create({get_name:function(){return 'jsname: foobar';},echo:function(s){return 'javascript method get_name called!' + s;}});})");
    
    animals.push_back((*animal_factories.find("mule")->second)());
    animals.push_back(new Animal());
    
    for(auto a : animals) {
        cout << a->get_name() << endl;
        cout << a->echo("test") << endl;
        cout << a->add(4,5)<<endl;
    }   
}