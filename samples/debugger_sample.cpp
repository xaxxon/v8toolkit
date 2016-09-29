#include "stdlib.h"
#include "debugger.h"

using namespace v8toolkit;

int main(int, char**) {
    Platform::init(0, nullptr);
    auto context = Platform::create_isolate()->create_context();

    auto script = context->compile("var a = 1;", "hard-coded-text-a.js");
    auto script2 = context->compile("var b = 2;", "hard-coded-text-b.js");

    Debugger debugger(context, 9002);
    for (;;) {
        debugger.poll();
        sleep(1);
    }
}