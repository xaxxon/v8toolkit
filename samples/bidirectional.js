var cpp_method_result = "C++ string";
var js_method_result = "JS string";

var thing = new Thing();
assert(thing.get_string() === cpp_method_result);

var subclassed_thing = subclass_thing({});
assert(subclassed_thing.get_string() === cpp_method_result);

var weird = Object.create(Object.create(subclassed_thing));
assert(weird.get_string() === cpp_method_result);

weird.get_string = function(){return js_method_result};
var weirder = Object.create(Object.create(weird));
assert(weirder.get_string() === js_method_result);


var subclassed_thing2 = subclass_thing({get_string: function(){return js_method_result}});
assert(subclassed_thing2.get_string() === js_method_result);

