#include <vector>
#include <map>
#include <functional>

#include "javascript.h"
#include "bidirectional.h"

using namespace std;
using namespace v8toolkit;

class Animal {
public:
    virtual ~Animal()=default;

    int i = rand();

    string get_type() {return "Cow";}
    virtual int get_name() {printf("In getname i = %d\n", i); return i;};
    virtual string echo(string s){return s;}
    virtual int add(int i, int j){return i + j;}
};


class JSAnimal : public Animal, public JSWrapper<Animal> {
public:
    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : JSWrapper(context, object) {}
    JS_ACCESS(int, get_name)
    JS_ACCESS_1(string, echo, string)
    JS_ACCESS_2(int, add, int, int)
};


template<class T>
class Factory {
public:
    virtual T * operator()() = 0;
};


// The first two parameters of the constructor for JSWrapperClass must be
//   a context and javascript object, then any other constructor parameters
template<class RealClass, class JSWrapperClass>
class JSFactory : public Factory<RealClass> {
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
    
    RealClass * operator()(){
        printf("In operator() with isolate %p\n", isolate);
        return scoped_run(isolate, global_context, [&](auto isolate, auto context){
            auto result = call_javascript_function(context, global_javascript_function.Get(isolate), context->Global());
            return new JSWrapperClass(context, v8::Local<v8::Object>::Cast(result));
        });
    }
};


map<string, std::unique_ptr<Factory<Animal>>> animal_factories;
vector<Animal*> animals;


void register_animal_factory(v8::Isolate * isolate, string type, v8::Local<v8::Function> factory_method) {
    printf("In registere animal factory got isolate: %p\n", isolate);
    animal_factories.emplace(type, make_unique<JSFactory<Animal, JSAnimal>>(isolate, v8::Local<v8::Function>::Cast(factory_method)));
}


int main(int argc, char ** argv)
{
    PlatformHelper::init(argc, argv);
       
    auto i = PlatformHelper::create_isolate();
    (*i)([&]{
        i->add_print();
        printf("Created isolate %p\n", i->get_isolate());
        auto & animal = i->wrap_class<Animal>();
        animal.add_method(&Animal::get_type, "get_type").add_method(&Animal::get_name, "get_name").add_member(&Animal::i, "i");
        animal.add_constructor("Animal", *i);
    
        // i->add_function("add_animal", &register_animal_factory);
    
        auto c = i->create_context();
    
        // c->run("add_animal('mule', function(){println('Returning new mule');return Object.create({get_name:function(){return 'jsname: foobar';},echo:function(s){return 'javascript method get_name called!' + s;}});})");
        // c->run("add_animal('horse', function(prototype){return function(){return Object.create(prototype)};}(new Animal()));");
        // c->run("a=new Animal();a.get_name();");
        c->run("c = new Animal(); c.get_name(); b=Object.create(c);c.get_name(); b.get_name();");

        
    
        // animals.push_back((*animal_factories.find("mule")->second)());
        // animals.push_back((*animal_factories.find("horse")->second)());
        
        // animals.push_back(new Animal());
    
        for(auto a : animals) {
            printf("About to run get_name\n");
            cout << a->get_name() << endl;
            printf("About to run echo\n");
            cout << a->echo("test") << endl;
            printf("About to run add\n");
            cout << a->add(4,5)<<endl;
        }   
    });
}

