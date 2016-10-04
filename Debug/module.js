module.exports.function = function(){return "module.js function output"};
module.exports.a = "a";
module.exports.five = 5;
println("This line of output expected once");

function module_function(){
    println("module_function INSIDE MODULE.JS");
}
