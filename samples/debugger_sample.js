
function c(){
    let c=10;
}

function b(some_number){
    some_number += 5;
    c(some_number);
}

function a(){
    let some_var = 5;
    some_var += 5;
    b(some_var);

}

a();
// println("In debugger_sample.js");