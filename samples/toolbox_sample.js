dumpdata=function(data){
	if(Array.isArray(data)) {
        return sprintf("[ %s ]", data.map(function(e){return dumpdata(e);}).join(", "));

    } else if (typeof(data)=="object") {
        return sprintf("{ %s }", Object.keys(data).map(function(k){
                return sprintf("%s: %s", k, dumpdata(data[k]));
        	}).join(", "));

    } else {
        return data;
    }
}



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

println("This module doesn't exist:");
require("this-module-does-not-exist.js");

var require_result = require("module.js");
require_result.function();

println("Rerunning same require, shouldn't print anything from inside the module");
var require_result = require("module.js");
println("Running the cached return value from requiring the same module");
require_result.function();

println(dumpdata(module_list()));


'yay'