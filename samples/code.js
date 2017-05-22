
// var p = new Point();
// println(p.x);
// println(p.y);
// println(p.stringthing());
// p.thing(4, "four");

// run garbage collector tests if javascript-exposed GC is enabled
assert (typeof gc == 'function');
gc(); 
var gc_test_1 = new Point();
assert(point_instance_count() === 1);

gc_test_1 = undefined;
gc();
assert(point_instance_count() === 0);


var line = new Line();
printobj(line.get_point());
let f = function(){}
println("Pringing traditional function:");
printobj(f);
line.get_point().get_foo();
assert(line.get_point().get_foo().i === 42);

println();


println("Testing printobj for object that is not a c++ wrapped object");
printobj({});
println();

// getting two objects by reference should have the same underlying c++ objects
//   because they return an object by reference

// These two objects should have the same backing c++ object
var p1 = line.get_point();
var p2 = line.get_point();
assert(p1 === p2);


// getting two objects by rvalue should have different underlying c++ objects
//   because they return on object as an rvalue that must have a copy made

// These two objects should have different backing c++ object
var l1 = line.get_point().get_foo();
var l2 = line.get_point().get_foo();
assert(l1 !== l2);

// Comparing objects obtained from data members of class types should have the same c++ object
var line_point_1 = line.p;
var line_point_2 = line.p;
assert(line_point_1 === line_point_2);


var override_method_point = new Point();
printobj(override_method_point);
println("About to run the original thing()");
override_method_point.thing(1, 'a');
override_method_point.thing = function(){println("This is not the original thing method!!")};
// printobj(override_method_point);
override_method_point.thing(1, 'b');


gc();
"yay"