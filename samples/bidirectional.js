var cpp_method_result = "C++ string";
var js_method_result = "JS string";

let thing2_i_val = 24;


let thing2_factory = create_thing_factory({}, 24);

// let thing2_factory = create_thing_factory(function(j_value, base_thing){
//     // println("base_thing i = " + base_thing.i);
//     if (typeof base_thing !== 'undefined') {
//         println("base_thing i = " + base_thing.i);
//
//     } else {
//         println("Base thing not defined");
//     }
//     return Thing.subclass({}, 24, j_value);
//
// }, 11);

println("HERE");
let count = 0;
let thing21 = thing2_factory.create("hello" + ++count);
let thing22 = thing2_factory.create("hello" + ++count);
let thing23 = thing2_factory.create("hello" + ++count);



// var thing = new Thing();
// assert(thing.get_string() ===cpp_method_result);
//
// var subclassed_thing = Thing.subclass({}, base_thing);
// assert(subclassed_thing.get_string() === cpp_method_result);
//
// var subclassed_thing2 = subclass_thing({get_string: function(){return js_method_result}});
// assert(subclassed_thing2.get_string() === js_method_result);

//
// var weird = Object.create(Object.create(subclassed_thing));
// assert(weird.get_string() === cpp_method_result);
//
// weird.get_string = function(){return js_method_result};
// var weirder = Object.create(Object.create(weird));
// assert(weirder.get_string() === js_method_result);


