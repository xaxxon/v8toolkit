module.exports.function = function(){return "module2.js function output"};
module.exports.a = "a";
module.exports.five = 5;
println("This output expected once");
// module_function(); // this isn't available from module.js since it was defined in the 'pretend' function wrapping
                      //    every module