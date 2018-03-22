#include <vector>
#include <map>
#include <functional>

#include <fmt/format.h>

#include "bidirectional.h"
#include "javascript.h"

//
//// eastl allocators
//void* operator new[](size_t size, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
//{
//	return malloc(size);
//}
//
//void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* pName, int flags, unsigned debugFlags, const char* file, int line)
//{
//	return malloc(size);
//}
//

using namespace std;
using namespace v8toolkit;



struct NonPolymorphicType : public v8toolkit::WrappedClassBase {};


struct Thing : public v8toolkit::WrappedClassBase {
	Thing(int i, const std::string & j): i(i), j(j) {
        std::cerr << fmt::format("Thing created at {}", (void*)this) << std::endl;
    }
	Thing(const Thing &) = delete;
	Thing& operator=(const Thing &) = delete;
	Thing(Thing &&) = delete;
	Thing & operator=(const Thing&&) = delete;
	virtual ~Thing(){}
	virtual std::string get_string(){return "C++ string";}
	virtual std::string get_string_value(){return "C++ string";}
	virtual std::string get_string_const()const{return "const C++ string";}
	
	std::string thing_only_function(){return "thing only function"; }

	// test case for non-polymorphic types
	NonPolymorphicType take_and_return_non_polymorphic(const NonPolymorphicType &) const {return NonPolymorphicType();}

	const int i = 42;
	std::string j = "forty-two";
};

struct JSThing : public Thing, public JSWrapper<Thing> {
    
    JSThing(int i, 
            const std::string & j) : 
        Thing(i, j), 
        JSWrapper(static_cast<Thing *>(this)) 
    {}
    
	JS_ACCESS(std::string, get_string, get_string);
	JS_ACCESS(std::string, get_string_value, get_string_value);
	JS_ACCESS_CONST(std::string, get_string_const, get_string_const);
};


class CreatedWithFactory : public WrappedClassBase {
    int i = 0;
    bool factory_method_called = false;
    CreatedWithFactory(int i) : i(i) {}
    
public:
    static CreatedWithFactory * make(int i) {
		std::cerr << fmt::format("in CreatedWithFactory::make") << std::endl;
        auto result = new CreatedWithFactory(i);
        result->factory_method_called = true;
        return result;
    }
    ~CreatedWithFactory() {
        std::cerr << fmt::format("CreatedWithFactory destructor running, verifying factory method used to construct object...") << std::endl;
        assert(this->factory_method_called);
    }
};

using PublicFactory = ConcreteFactory<Thing, TypeList<int>, TypeList<std::string const &>>;

PublicFactory thing_factory_3(PublicFactory::CppFactoryInfo<Thing>(), 3);
//using BaseFactory = Factory<Thing, TypeList<int>, TypeList<std::string const &>>;
//using JSThingFactory = JSFactory<Thing, JSThing, TypeList<int>, TypeList<const std::string &>>;
//using ThingFactory = CppFactory<Thing, Thing, TypeList<int>, TypeList<std::string const &>>;
//static_assert(std::is_base_of_v<BaseFactory, ThingFactory>);
//static_assert(std::is_base_of_v<BaseFactory, JSThingFactory>);
static vector<std::unique_ptr<PublicFactory>> thing_factories;


void create_thing_factory(const v8::FunctionCallbackInfo<v8::Value> & info) {
	auto isolate = info.GetIsolate();

	thing_factories.push_back(std::make_unique<PublicFactory>(
		PublicFactory::JSFactoryInfo<JSThing>(
			get_value_as<PublicFactory *>(info[0]),
			get_value_as<v8::Object>(info[1]),
			get_value_as<v8::Function>(info[2])
		), CastToNative<int>()(isolate, info[3])));

	// Since the newly created type is not being named, we don't skip any parameters <0>.  If the javascript caller was going to
	// provide a type/factory name as the first parameter, then it would be <1>
//	thing_factories.emplace_back(JSThingFactory::create_factory_from_javascript<0>(info));
	
	// return the factory back to the caller
	info.GetReturnValue().Set(CastToJS<PublicFactory*>()(isolate, thing_factories.back().get()));
}


void clear_thing_factories(){
	thing_factories.clear();
}



void test_calling_bidirectional_from_javascript()
{
	auto isolate = Platform::create_isolate();
	
	(*isolate)([&]{
		isolate->add_assert();
		isolate->add_print();
		auto & thing = isolate->wrap_class<Thing>();
		thing.add_method("get_string", &Thing::get_string);
		thing.add_method("get_string_value", &Thing::get_string_value);
		thing.add_method("get_string_const", &Thing::get_string_const);
		thing.add_method("take_and_return_non_polymorphic", &Thing::take_and_return_non_polymorphic);
		thing.add_method("thing_only_function", &Thing::thing_only_function);
		thing.set_compatible_types<JSThing>();
		thing.add_member_readonly<&Thing::i>("i");
		thing.add_member<&Thing::j>("j");

		thing.finalize();
		thing.add_constructor<int, const std::string &>("Thing", *isolate);
		
		auto & jsthing = isolate->wrap_class<JSThing>();
		jsthing.set_parent_type<Thing>();
		jsthing.finalize();

        PublicFactory::wrap_factory(*isolate);		

		auto & non_polymoprhic = isolate->wrap_class<NonPolymorphicType>();
		non_polymoprhic.finalize();


		auto context = isolate->create_context();

		{
			CONTEXT_SCOPED_RUN(context->get_context());

            PublicFactory * base_factory_3 = &thing_factory_3;
			context->expose_variable("thing_factory_3", base_factory_3);
			context->add_function("create_thing_factory", &create_thing_factory);

			context->run_from_file("bidirectional.js");

			// need to clear these up before the context/isolates go away
			thing_factories.clear();
		}
	});
}






class Animal {
	std::string name;
public:
	Animal(const std::string & name) : name(name) {}
	Animal(const Animal&) = default;
	Animal(Animal&&) = delete;
	Animal& operator=(const Animal&) = delete;
	Animal& operator=(Animal&&) = delete;

	virtual ~Animal()=default;

    int i = 42;

    virtual string get_name() const {return name;}
    virtual string get_type() const {return "cow";}
    virtual int get_i() const {return i;};
    virtual string echo(const Animal & s) const {return "Animal::echo";}
    virtual int add(int i, int j) const {return i + j;}
};

class Zebra : public Animal {
public:
	Zebra(const std::string & name) : Animal(name) {}
    virtual string get_type() const override {return "zebra";}
	~Zebra(){}
};


//
//
//class JSAnimal : public Animal, public JSWrapper<Animal> {
//public:
//    JSAnimal(v8::Local<v8::Context> context, v8::Local<v8::Object> object, v8::Local<v8::FunctionTemplate> function_template, const std::string & name) :
//		Animal(name),
//		JSWrapper(context, object, function_template)
//	{}
//
//	~JSAnimal(){}
//
//    // Every function you want to override in javascript must be in this list or it will ALWAYS call the C++ version
//    JS_ACCESS_CONST(string, get_name);
//    JS_ACCESS_CONST(int, get_i);
//    JS_ACCESS_1_CONST(string, echo, const Animal &);
//    JS_ACCESS_2_CONST(int, add, int, int);
//    JS_ACCESS_CONST(string, get_type);
//        void crap(){}
//};
//
//
//#define ANIMAL_CONSTRUCTOR_ARGS const std::string &
//using AnimalFactory = Factory<Animal, TypeList<ANIMAL_CONSTRUCTOR_ARGS>>;
//using JSAnimalFactory = JSFactory<Animal, JSAnimal, TypeList<>, TypeList<ANIMAL_CONSTRUCTOR_ARGS>>;
//
//template <class T>
//using CppAnimalFactory = CppFactory<Animal, T, TypeList<ANIMAL_CONSTRUCTOR_ARGS>>;
//
//
//map<string, std::unique_ptr<AnimalFactory>> animal_factories;
//vector<Animal*> animals;
//
//
//void register_animal_factory(v8::Local<v8::Context> context, string type, v8::Local<v8::Function> factory_method) {
//    animal_factories.emplace(type, make_unique<JSAnimalFactory>(context, factory_method));
//}


int main(int argc, char ** argv)
{
    Platform::init(argc, argv, ".");
	
	printf("Calling TCBFJ\n");
    test_calling_bidirectional_from_javascript();


    {
        ConcreteFactory <CreatedWithFactory, TypeList<>, TypeList<int>> 
            factory((ConcreteFactory < CreatedWithFactory,
                    TypeList<>, TypeList < int >> ::CppFactoryInfo<CreatedWithFactory, &CreatedWithFactory::make>()));
        
        delete factory(4);
    }
    
     exit(0);
//    auto i = Platform::create_isolate();
//    (*i)([&]{
//		printf("Running 'main' tests\n");
//        i->add_assert();
//		i->add_print();
//
//        auto & animal = i->wrap_class<Animal>();
//        animal.add_method("get_type", &Animal::get_type);
//	animal.add_method("get_name", &Animal::get_name);
//        animal.add_method("get_i", &Animal::get_i);
//		animal.add_method("echo", &Animal::echo);
//		animal.add_method("add", &Animal::add);
//        // animal.add_member("i", &Animal::i);
//		animal.set_compatible_types<JSAnimal>();
//        animal.finalize();
//        animal.add_constructor<const std::string &>("Animal", *i);
//
//		auto & jsanimal = i->wrap_class<JSAnimal>();
//        jsanimal.add_method("crap", &JSAnimal::crap);
//		jsanimal.set_parent_type<Animal>();
//		jsanimal.finalize();
//
//		JSAnimalFactory::add_subclass_function(*i, *i, "subclass_animal");
//        i->add_function("add_animal_factory", &register_animal_factory);
//
//        auto c = i->create_context();
//
//        // add new animal factories from javascript
//		c->run("add_animal_factory('mule', function(name){var foo = subclass_animal({get_type:function(){return 'mule'},\
//																				add:function(a,b){return a + b + this.get_i()}, \
//																				get_i:function(){return 1}, \
//																				echo:function(s){printobj(s); return 'js-mule-echo: ' + s.get_name();}}, name \
//																			 ); println('inline test', foo.get_type()); return foo;})");
//
//        animal_factories.insert(pair<string, std::unique_ptr< AnimalFactory >>("zebra", make_unique< CppAnimalFactory<Zebra> >()));
//
//
//        // create animals based on the registered factories
//        animals.push_back((*animal_factories.find("mule")->second)("Mandy the Mule"));
//        animals.push_back((*animal_factories.find("zebra")->second)("Zany Zebra"));
//
//        assert(animals.size() == 2);
//
//        // mule
//        auto & mule = animals[0];
//        assert(mule->get_type() == "mule");
//        assert(mule->get_i() == 1);
//	Animal monkey("monkey");
//        assert(mule->echo(monkey) == "js-mule-echo: monkey");
//        assert(mule->add(2,2) == 5); // mules don't add well
//
//
//        // Checking Zebra
//        auto & zebra = animals[1];
//        assert(zebra->get_type() == "zebra");
//        assert(zebra->get_i() == 42);
//        assert(zebra->echo(monkey) == "Animal::echo");
//        assert(zebra->add(2,2) == 4); // zebras are good at math
//
//    });
	
	printf("Bidirectional tests successful\n");
}

