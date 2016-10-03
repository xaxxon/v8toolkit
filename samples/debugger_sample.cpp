#include "stdlib.h"
#include "debugger.h"

using namespace v8toolkit;

int main(int, char**) {
    Platform::expose_debug_as("v8debug");
    Platform::init(0, nullptr);
    auto isolate = Platform::create_isolate();
    isolate->add_print();
    auto context = isolate->create_context();

    auto script = context->compile("a = 1;\r\na+=1;", "hard-coded-text-a.js");
    auto script2 = context->compile("b = 2;\r\nb+=2;", "hard-coded-text-b.js");
    const char * debugger_sample_js_filename = "debugger_sample.js";
    std::string script_3_source;
    assert(v8toolkit::get_file_contents(debugger_sample_js_filename, script_3_source));
    auto script3 = context->compile(script_3_source, debugger_sample_js_filename);

    // set up chrome debug protocol compatible http server on port 9002
    // To connect, start chrome with --remote-debugging-port=9222 http://localhost:9222/devtools/inspector.html?ws=localhost:9002
    // The debugger javascript application is served from chrome, but then it connects to our websocket
    Debugger debugger(context, 9002);
    for (;;) {
        script3->run();
        debugger.poll();
        usleep(1000000);
    }
}