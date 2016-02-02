#include <stdio.h>

#include "javascript.h"

using namespace v8toolkit;


int main(int argc, char ** argv) {
    
    PlatformHelper::init(argc, argv);
    auto i = PlatformHelper::create_isolate();
    auto c = i->create_context();
    try{
        c->run("doesnotexist.doesnotexist");
    } catch(...){
        printf("caught doesnotexist.doesnotexist exception\n");
    }   
    try{
        c->run("throw 4;");
    } catch(...){
        printf("caught throw 4 exception\n");
    }   
    try{
        c->run("throw {a:4, b:3}");
    } catch(...){
        printf("caught throw 4 exception\n");
    }   
    
}