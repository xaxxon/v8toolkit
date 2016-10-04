println("Bar");
bar();
println("foo5");
foo(5);
println("foo2");
foo(2);
println("assert point instance count 1");
println(point_instance_count());
assert(point_instance_count() == 1);
println("exposed var 42");
println(exposed_variable);
assert(exposed_variable === 42);
exposed_variable = 420;
println("exposed var 420");
assert(exposed_variable === 420);

// This line should be ignored
exposed_variable_readonly=4;
println("exposed var still 420");
assert(exposed_variable === 420);

println("Lambda func 100 => 101");
assert(lambda_function(100) == 101);

println("Testing print functions:")
printfln("This should print out '1 a 2 b 3 c': %d a %d b %d c", [1,2,3]);
println("This should print out '1 2 3': ", [1,2,3]);


function some_global_function(){
	println("In some global function");
}

var caught_expected_exception = false;
try {
    require("this-module-does-not-exist.js");
} catch (e) {
    caught_expected_exception = true;
}
assert(caught_expected_exception);

var require_result = require("module.js");
assert(require_result.a === "a");
assert(require_result.five === 5);
assert(require_result.function !== undefined);
assert(require_result.bogus === undefined);
assert(require_result.function() === "module.js function output");

var require_result = require("module.js");
assert(require_result.function() === "module.js function output");


var module2_results = require("module2.js");
assert(module2_results.function() === "module2.js function output");
// printobj(module2_results);
// printobj(module_list());

// must return "yay" for this test
"yay"