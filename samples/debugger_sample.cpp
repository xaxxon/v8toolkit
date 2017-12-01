
/*

 clang++ -std=c++17 debugger_sample.cpp  -I ../../v8/include/ -I ../include/v8toolkit/ -I /usr/local/include -DXL_USE_PCRE -DXL_USE_LIB_FMT -lv8toolkit_shared -lboost_system -lpcre -L /Users/xaxxon/v8libs/x64.debug.shared/ -licui18n -licuuc -lv8 -lv8_libbase -lv8_libplatform  -g

*/



#include <regex>

#include "stdlib.h"
#include "debugger.h"
#include "log.h"

using namespace v8toolkit;

int main(int argc, char** argv) {

    v8toolkit::Platform::init(argc, argv, argv[1]);
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

        context->require_directory("modules");


        using namespace v8toolkit::literals;
        v8::ScriptOrigin script_origin(v8::String::NewFromUtf8(*isolate, context->get_url("test/path/compile_function_in_context").c_str()),
                                       0_v8, // line offset
                                       12_v8, // column offset - stupid having to shift it over for the string length that gets put on it
                                       v8::Local<v8::Boolean>(), // resource_is_shared_cross_origin
                                        100_v8);


        v8::ScriptCompiler::Source source("println(\"in code from CompileFunctionInContext\");"_v8, script_origin);
        auto maybe_function = v8::ScriptCompiler::CompileFunctionInContext(*context, &source, 0, nullptr, 0, nullptr);
        assert(!maybe_function.IsEmpty());
        auto function = maybe_function.ToLocalChecked();
        std::cerr << fmt::format("infinite loop waiting for debugger operations") << std::endl;
        while (true) {
            (void)function->Call(*context, context->get_context()->Global(), 0, nullptr);
            script3->run();
            context->get_channel().poll();
            usleep(1000000);
        }
    });
}
