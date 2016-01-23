bar();
foo(5);
foo(2);
point_instance_count();

println("should be 42: ", exposed_variable);
exposed_variable = 420;
println("Should be 420: ", exposed_variable);

// This line should be ignored
exposed_variable_readonly=4;
printfln("This should still be 420: %d", exposed_variable_readonly);


printfln("This should be 101: %d",lambda_function(100));

printfln("This should print out '1 a 2 b 3 c': %d a %d b %d c", [1,2,3]);
println("This should print out '1 2 3': ", [1,2,3]);


function some_global_function(){
	println("In some global function");
}
var require_result = require("module.js");
require_result();

'yay'