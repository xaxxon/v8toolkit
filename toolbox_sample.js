bar();
foo(5);
foo(2);
point_instance_count();

println(exposed_variable);
exposed_variable = 420;
println(exposed_variable);

// This line should be ignored
exposed_variable_readonly=4;	
printfln("This should still be 420: %d", exposed_variable_readonly);