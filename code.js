var line = new Line();

println("line.get_point().get_foo().i = %d", line.get_point().get_foo().i);


println("This object is not a c++ wrapped object");
printobj({});

// getting two objects by reference should have the same underlying c++ objects
//   because they return an object by reference
println("These two objects should have the same backing c++ object");
var p1 = line.get_point();
var p2 = line.get_point();

printobj(p1);
printobj(p2);
println("These objects should be the same javascript object as well: %s", p1===p2?"same": "different");

// getting two objects by rvalue should have different underlying c++ objects
//   because they return on object as an rvalue that must have a copy made
println("These two objects should have different backing c++ object");
var l1 = line.get_point().get_foo();
var l2 = line.get_point().get_foo();
printobj(l1);
printobj(l2);
println("These objects should not be the same javascript object, either: %s", l1===l2?"same": "different");


// run the garbage collector, will fail unless javascript-side garbage collection is explicitly enabled
if (typeof gc == 'function') {
	gc();
	println("Testing garbage collection, one Point object (but not two) should be GC'd and deleted");
	var gc_test_1 = new Point();
	gc_test_1 = undefined;
	gc();
	println("Done running GC");
} else {
	println("Not running garbage collector from javascript because it's not exposed via --enable-gc")
}

