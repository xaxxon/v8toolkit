#include <vector>
#include <map>
#include <functional>

#include "javascript.h"
#include "bidirectional.h"

using namespace std;
using namespace v8toolkit;


struct Thing {
	Thing(){printf("Creating Thing\n");}
	virtual ~Thing(){}
	virtual std::string get_string(){return "C++ string";}
};

struct JSThing : public Thing, public JSWrapper<Thing> {
	JSThing(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::FunctionTemplate> function_template) : JSWrapper(context, object, function_template) {printf("Creating JSThing\n");}
	JS_ACCESS(std::string, get_string);
};

void test_calling_bidirectional_from_javascript()
{
	auto isolate = Platform::create_isolate();
	
	(*isolate)([&]{
		isolate->add_assert();
		isolate->add_print();
		auto & thing = isolate->wrap_class<Thing>();
		thing.add_method("get_string", &Thing::get_string);
		thing.set_compatible_types<JSThing>();
		thing.finalize();
		thing.add_constructor("Thing", *isolate);
		
		auto & jsthing = isolate->wrap_class<JSThing>();
		jsthing.set_parent_type<Thing>();
		jsthing.finalize();
		JSFactory<Thing, JSThing>::add_subclass_function(*isolate, *isolate, "subclass_thing");
		

		auto context = isolate->create_context();

		context->run_from_file("bidirectional.js");
	});
}






class Animal {
	std::string name;
public:
	Animal(const std::string & name) : name(name) {}
    virtual ~Animal()=default;

    int i = 42;

    virtual string get_type() {return "cow";}
    virtual int get_i() {return i;};
    virtual string echo(string s){return s;}
    virtual int add(int i, int j){return i + j;}
};

class Zebra : public Animal {
public:
	Zebra(const std::string & name) : Animal(name) {}
    virtual string get_type() override {return "zebra";}
	~Zebra(){}
};




class JSAnimal : public Animal, public JSWrapper<Animal> {
public:
    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::FunctionTemplate> function_template, const std::string & name) : 
		Animal(name), 
		JSWrapper(context, object, function_template) 
	{}
		
	~JSAnimal(){}
    
    // Every function you want to override in javascript must be in this list or it will ALWAYS call the C++ version
    JS_ACCESS(int, get_i)
    JS_ACCESS_1(string, echo, string)
    JS_ACCESS_2(int, add, int, int)
    JS_ACCESS(string, get_type)
        void crap(){}
};


#define ANIMAL_CONSTRUCTOR_ARGS const std::string &
using AnimalFactory = Factory<Animal, ANIMAL_CONSTRUCTOR_ARGS>;
using JSAnimalFactory = JSFactory<Animal, JSAnimal, ANIMAL_CONSTRUCTOR_ARGS>;

template <class T>
using CppAnimalFactory = CppFactory<Animal, T, ANIMAL_CONSTRUCTOR_ARGS>;


map<string, std::unique_ptr<AnimalFactory>> animal_factories;
vector<Animal*> animals;


void register_animal_factory(v8::Local<v8::Context> context, string type, v8::Local<v8::Function> factory_method) {
    animal_factories.emplace(type, make_unique<JSAnimalFactory>(context, factory_method));
}


int main(int argc, char ** argv)
{
    Platform::init(argc, argv);
	
	printf("Calling TCBFJ\n");
    test_calling_bidirectional_from_javascript();
    // exit(0);
    auto i = Platform::create_isolate();
    (*i)([&]{
		printf("Running 'main' tests\n");
        i->add_assert();
		i->add_print();

        auto & animal = i->wrap_class<Animal>();
        animal.add_method("get_type", &Animal::get_type);
        animal.add_method("get_i", &Animal::get_i);
		animal.add_method("echo", &Animal::echo);
		animal.add_method("add", &Animal::add);
        // animal.add_member("i", &Animal::i);
		animal.set_compatible_types<JSAnimal>();
        animal.finalize();
        animal.add_constructor<const std::string &>("Animal", *i);
		
		auto & jsanimal = i->wrap_class<JSAnimal>();
        jsanimal.add_method("crap", &JSAnimal::crap);
		jsanimal.set_parent_type<Animal>();
		jsanimal.finalize();

		JSAnimalFactory::add_subclass_function(*i, *i, "subclass_animal");
        i->add_function("add_animal_factory", &register_animal_factory);
    
        auto c = i->create_context();
    
        // add new animal factories from javascript
		c->run("add_animal_factory('mule', function(){var foo = subclass_animal({get_type:function(){return 'mule'},\
																				add:function(a,b){return a + b + this.get_i()}, \
																				get_i:function(){return 1}, \
																				echo:function(s){return 'js-mule-echo: ' + s;}} \
																			 ); println('inline test', foo.get_type()); return foo;})");
		
        c->run("add_animal_factory('horse', function(prototype){return function(){return subclass_animal(prototype)}}(new Animal()))");
        animal_factories.insert(pair<string, std::unique_ptr< AnimalFactory >>("zebra", make_unique< CppAnimalFactory<Zebra> >()));
        

        // create animals based on the registered factories
        animals.push_back((*animal_factories.find("mule")->second)("Mandy the Mule"));
        animals.push_back((*animal_factories.find("horse")->second)("Henry the Horse"));
        animals.push_back((*animal_factories.find("zebra")->second)("Zany Zebra"));

        assert(animals.size() == 3);
        
        // mule
        auto a = animals[0];
		printf("About to print a->get_type\n");
        printf("a->get_type: %s\n", a->get_type().c_str());
        assert(a->get_type() == "mule");
        assert(a->get_i() == 1);
        assert(a->echo("mule") == "js-mule-echo: mule");
        assert(a->add(2,2) == 5); // mules don't add well

        // the "horse" object doesn't overload anything, so it's really a cow
        a = animals[1];
        assert(a->get_type() == "cow");
        assert(a->get_i() == 42);
        assert(a->echo("horse") == "horse");
        assert(a->add(2,2) == 4); // cows are good at math
        
        // Checking Zebra
        a = animals[2];
        assert(a->get_type() == "zebra");
        assert(a->get_i() == 42);
        assert(a->echo("zebra") == "zebra");
        assert(a->add(2,2) == 4); // cows are good at math
        
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
	
	printf("Bidirectional tests successful\n");
}

