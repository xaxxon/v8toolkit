println("THIS CODE IS IN MODULE.JS");

println("This should print 'in some global function' which is defined outside the module");
printobj(global.module);

module.exports = function(){println("This message is from inside the function returned from module.js");};