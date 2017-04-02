function c(){
    let c=10;
}

function b(some_number){
    some_number += 5;
    c(some_number);
}

function a(){
    //println("Beginning of a()");
    let some_var = 5;
    some_var += 5;
    b(some_var);
   // println("End of a()");

}

a();
// println("In debugger_sample.js");