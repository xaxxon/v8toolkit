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

    int i = 42;

    virtual string get_type() {return "cow";}
    virtual int get_i() {return i;};
    virtual string echo(string s){return s;}
    virtual int add(int i, int j){return i + j;}
};

class Zebra : public Animal {
    virtual string get_type() override {return "zebra";}
};


class JSAnimal : public Animal, public JSWrapper<Animal> {
public:
    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object) : JSWrapper(context, object) {}
    
    // Every function you want to override in javascript must be in this list or it will ALWAYS call the C++ version
    JS_ACCESS(int, get_i)
    JS_ACCESS_1(string, echo, string)
    JS_ACCESS_2(int, add, int, int)
    JS_ACCESS(string, get_type)
};




map<string, std::unique_ptr<FactoryBase<Animal>>> animal_factories;
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
        i->add_assert();

        auto & animal = i->wrap_class<Animal>();
        animal.add_method(&Animal::get_type, "get_type");
        animal.add_method(&Animal::get_i, "get_i");
        animal.add_member(&Animal::i, "i");
        animal.finalize();
        animal.add_constructor("Animal", *i);
    
        i->add_function("add_animal", &register_animal_factory);
    
        auto c = i->create_context();
    
        // add new animal factories from javascript
        c->run("add_animal('mule', function(){println('Returning new mule');return Object.create({get_type:function(){return 'mule'},add:function(a,b){return a + b + this.get_i();}, get_i:function(){return 1;},echo:function(s){return 'js-mule-echo: ' + s;}});})");
        c->run("add_animal('horse', function(prototype){return function(){return Object.create(prototype)};}(new Animal()));");
        animal_factories.insert(pair<string, std::unique_ptr< FactoryBase<Animal> >>("zebra", make_unique< Factory<Zebra, Animal> >()));
        
        // create subclass of Animal in javascript and test that methods still work
        c->run("a=new Animal();a.get_i();");
        c->run("c = new Animal(); c.get_i(); b=Object.create(c);c.get_i(); b.get_i();");


        // create animals based on the registered factories
        animals.push_back((*animal_factories.find("mule")->second)());
        animals.push_back((*animal_factories.find("horse")->second)());
        animals.push_back((*animal_factories.find("zebra")->second)());
        
        assert(animals.size() == 3);
        
        // mule
        printf("Checking Mule:\n");
        auto a = animals[0];
        assert(a->get_type() == "mule");
        assert(a->get_i() == 1);
        assert(a->echo("mule") == "js-mule-echo: mule");
        assert(a->add(2,2) == 5); // mules don't add well
        
        // the "horse" object doesn't overload anything, so it's really a cow
        printf("Checking Horse:\n");
        a = animals[1];
        assert(a->get_type() == "cow");
        assert(a->get_i() == 42);
        assert(a->echo("horse") == "horse");
        assert(a->add(2,2) == 4); // cows are good at math
        
        // Checking Zebra
        printf("Checking zebra\n");
        a = animals[2];
        assert(a->get_type() == "zebra");
        assert(a->get_i() == 42);
        assert(a->echo("zebra") == "zebra");
        assert(a->add(2,2) == 4); // cows are good at math
        printf("Barn looks good\n");
        
        

        //
        //
        // // horse
        //
        //     printf("About to run get_i\n");
        //
        //     cout << a->get_i() << endl;
        //     printf("About to run echo\n");
        //     cout << a->echo("test") << endl;
        //     printf("About to run add\n");
        //     cout << a->add(4,5)<<endl;   
    });
}

