#include "stdlib.h"
#include "debugger.h"

using namespace v8toolkit;

int main(int, char**) {
    Platform::init(0, nullptr);
    auto context = Platform::create_isolate()->create_context();


    Debugger debugger(context, 9002);
    for (;;) {
        debugger.poll();
        sleep(1);
    }
}