#include "stdlib.h"
#include "debugger.h"

using namespace v8toolkit;

int main(int, char**) {
//    v8toolkit::Platform::expose_debug_as("v8debug");
    v8toolkit::Platform::init(0, nullptr);
    auto isolate = v8toolkit::Platform::create_isolate();
    isolate->add_print();
    auto context = isolate->create_context();

    auto script = context->compile("a = 1;\r\na+=1;", "hard-coded-text-a.js");
    auto script2 = context->compile("b = 2;\r\nb+=2;", "hard-coded-text-b.js");
    const char * debugger_sample_js_filename = "debugger_sample.js";
    std::string script_3_source;
    assert(v8toolkit::get_file_contents(debugger_sample_js_filename, script_3_source));
    auto script3 = context->compile(script_3_source, debugger_sample_js_filename);

    ISOLATE_SCOPED_RUN(*isolate);






    InspectorClient client(*context, 9002);
    auto session = client.GetSession(*context);

    using namespace v8toolkit::literals;
    v8::ScriptOrigin script_origin(v8::String::NewFromUtf8(*isolate, (std::string("v8toolkit://") + context->get_uuid_string() + "/" + "compile_function_in_context").c_str()), 1_v8);
    v8::ScriptCompiler::Source source("println(\"in code from CompileFunctionInContext\");"_v8, script_origin);
    auto maybe_function = v8::ScriptCompiler::CompileFunctionInContext(*context, &source, 0, nullptr, 0, nullptr);
    assert(!maybe_function.IsEmpty());
    auto function = maybe_function.ToLocalChecked();
    std::cerr << fmt::format("infinite loop waiting for debugger operations") << std::endl;
    while(true) {
        script3->run();
        client.channel_->poll();
        usleep(1000000);
    }
}