

#include <fstream>
#include <sstream>

#include "../clang.h"
#include "../ast_action.h"

int main() {
    std::ifstream sample_source("sample.cpp");
    std::string sample_source_contents((std::istreambuf_iterator<char>(sample_source)),
                    std::istreambuf_iterator<char>());

    clang::tooling::runToolOnCode(new v8toolkit::class_parser::PrintFunctionNamesAction, sample_source_contents);
}