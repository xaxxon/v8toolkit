#include <regex>

#include "stdlib.h"
#include "debugger.h"

using namespace v8toolkit;

int main(int argc, char** argv) {

    v8toolkit::Platform::init(argc, argv, argv[0]);
    auto isolate = v8toolkit::Platform::create_isolate();
    isolate->add_print();
    auto context = isolate->create_debug_context(9002);

    context->get_channel().wait_for_connection();

    auto script = context->compile("a = 1;\r\na+=1;", "hard-coded-text-a.js");
    auto script2 = context->compile("b = 2;\r\nb+=2;", "hard-coded-text-b.js");
    const char * debugger_sample_js_filename = "debugger_sample.js";
    std::string script_3_source;
    bool result = v8toolkit::get_file_contents(debugger_sample_js_filename, script_3_source);
    assert(result);
    auto script3 = context->compile(script_3_source, debugger_sample_js_filename);

    (*context)([&] {

        require_directory(*context, "modules");


        using namespace v8toolkit::literals;
        v8::ScriptOrigin script_origin("compile_function_in_context.js"_v8,
                                       0_v8, // line offset
                                       12_v8, // column offset
                                       v8::Local<v8::Boolean>(), // resource_is_shared_cross_origin
                                        100_v8);


        v8::ScriptCompiler::Source source("println(\"in code from CompileFunctionInContext\");"_v8, script_origin);
        auto maybe_function = v8::ScriptCompiler::CompileFunctionInContext(*context, &source, 0, nullptr, 0, nullptr);
        assert(!maybe_function.IsEmpty());
        auto function = maybe_function.ToLocalChecked();
        std::cerr << fmt::format("infinite loop waiting for debugger operations") << std::endl;
        while (true) {
            function->Call(*context, context->get_context()->Global(), 0, nullptr);
            script3->run();
            context->get_channel().poll();
            usleep(1000000);
        }
    });
}