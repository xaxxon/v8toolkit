
#include "../clang.h"
#include "../ast_action.h"

int main() {


    clang::tooling::runToolOnCode(new v8toolkit::class_parser::PrintFunctionNamesAction, "class X {};");
}