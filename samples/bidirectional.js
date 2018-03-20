var cpp_method_result = "C++ string";
var js_method_result = "JS string";

let thing2_i_val = 24;


let thing_factory_4 = create_thing_factory(
    thing_factory_3,
    {
        get_string: function(){return js_method_result;},
        thing_factory_4_function: function(){},

    }, // This is the actual prototype object shared by all new instances of the type
    function(str){ // This function is called on each new object, creating non-shared, per-object properties
        this.per_object_property = str;
    }, 4);


let thing_factory_5 = create_thing_factory(
    thing_factory_4,
    {
        thing_factory_5_function: function(){},
    },
    function(str){
        this.per_object_property = str;      
    }, 5);



var thing = new Thing(42, "test");
printobj(thing);
assert("thing.get_string() === cpp_method_result");
assert("thing.thing_only_function() == \"thing only function\"");


let count = 0;
let thing41 = thing_factory_4.create(++count);

assert("thing41.get_string() == js_method_result");
assert("thing41.per_object_property == 1");

println("Creating thing4 - should only be one Thing created before 'thing42'");
let thing42 = thing_factory_4.create(++count);
println("thing42");
printobjall(thing42);
assert("thing42.i == 4");
assert("thing42.per_object_property == 2");

thing41.per_object_property++;
assert("thing41.per_object_property == 2");
assert("thing42.per_object_property == 2");


let thing51 = thing_factory_5.create(++count);
assert("thing51.i == 5");



//
// var subclassed_thing = Thing.subclass({}, base_thing);
// assert("subclassed_thing.get_string() === cpp_method_result");
//
// var subclassed_thing2 = subclass_thing({get_string: function(){return js_method_result}});
// assert("subclassed_thing2.get_string() === js_method_result");

//
// var weird = Object.create(Object.create(subclassed_thing));
// assert("weird.get_string() === cpp_method_result");
//
// weird.get_string = function(){return js_method_result};
// var weirder = Object.create(Object.create(weird));
// assert("weirder.get_string() === js_method_result");


