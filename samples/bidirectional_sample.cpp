#include <vector>
#include <map>
#include <functional>

#include "javascript.h"

using namespace std;
using namespace v8toolkit;

    

class Animal {
public:
    string get_type() {return "Cow";}
    virtual string get_name() {return "This is the c++ get_name";};
};

class JSWrapper {
protected:
    v8::Isolate * isolate;
    v8::Global<v8::Context> global_context;
    v8::Global<v8::Object> global_js_object;
    
public:
    JSWrapper(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : 
        isolate(context->GetIsolate()), 
        global_context(v8::Global<v8::Context>(isolate, context)),
        global_js_object(v8::Global<v8::Object>(isolate, object)) {}
};


class JSAnimal : public Animal, public JSWrapper {
public:
    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : JSWrapper(context, object) {}
    
    virtual string get_name() override {
        return scoped_run(isolate, global_context,[this](auto isolate, auto context){
            v8::TryCatch tc(isolate);
            auto js_object = global_js_object.Get(isolate);
            // load up the attribute by name
            auto maybe_function_value = js_object->Get(context, v8::String::NewFromUtf8(isolate, "get_name"));
            
            // if there is nothing with the matching attribute name
            if(!maybe_function_value.IsEmpty()) {
                printf("object.Get returned a value\n");
                if(!maybe_function_value.ToLocalChecked()->IsUndefined()) {
                
                    auto jsfunction = v8::Local<v8::Function>::Cast(maybe_function_value.ToLocalChecked());
                
                    // if the matching attribute name isn't a function
                    if(jsfunction.IsEmpty()) {
                        assert(false); // actually, call the C++ implementation
                    }
                    // TODO: need to make the next line's Call() with any appropriate parameters
                    // print_v8_value_details(js_object);
                    auto maybe_result = jsfunction->Call(context, js_object, 0, nullptr);

                    // was there an exception while calling the function
                    if(tc.HasCaught()) {
                        assert(false);
                    }
                    auto result = maybe_result.ToLocalChecked();
                    return CastToNative<string>()(isolate, result);
                }
            }
            return this->Animal::get_name();
        });
    }  
};


template<class T>
class Factory {
public:
    virtual T * operator()() = 0;
};




// method that returns an element from the list of objects should check and see if it's a JSObject or C++ Impl object and return
//   the right type based on being parameterizsed with both types somehow




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
    
    c->run("add_animal('mule', function(){println('Returning new mule');return Object.create({xget_name:function(){return 'javascript method get_name called!';}});})");
    
    animals.push_back((*animal_factories.find("mule")->second)());
    animals.push_back(new Animal());
    
    for(auto a : animals) {
        cout << "Trying to print get_name()"<<endl;
        cout << a->get_name() << endl;
    }   
}