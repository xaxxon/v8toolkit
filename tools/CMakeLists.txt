
set(CMAKE_CXX_STANDARD 14)

set(CLANG_HOME $ENV{CLANG_HOME})

link_directories(${CLANG_HOME}/lib  )
include_directories(${V8_INCLUDE_DIR})
include_directories(${CLANG_HOME}/include)
set(CXX_COMPILER ${CLANG_HOME}/bin)

#;;;; NEED TO ADD LDFLAGS PROGRAM THING!!!  `$CLANG_HOME/bin/llvm-config --ldflags --libs --system-libs`
execute_process(COMMAND ${CLANG_HOME}/bin/llvm-config --ldflags --libs --system-libs
        OUTPUT_VARIABLE output_var
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

string(REPLACE "\n" " " output_var ${output_var})


#;;;; NEED TO ADD -fno-rtti to compilation -- line below not working
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -pedantic" )
set(CMAKE_SHARED_LINKER_FLAGS ${output_var})

add_library(class_parser SHARED class_parser.cpp)


# how to add the result of this to the compile command?
# `$CLANG_HOME/bin/llvm-config --ldflags --libs --system-libs`

# Not actually sure which of these are actually needed, but some of them are
target_link_libraries(class_parser clangFrontend clangSerialization clangASTMatchers clangSema
        clangAnalysis clangRewriteFrontend clangEdit clangDriver clangLex
        LLVMSymbolize LLVMDebugInfoPDB LLVMScalarOpts LLVMInstCombine LLVMInstrumentation LLVMBitWriter
        LLVMTransformUtils LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMRuntimeDyld
        LLVMObject LLVMBitReader LLVMCore LLVMSupport clangParse clangAST
        clangBasic clangParse)

target_link_libraries(class_parser fmt)


add_custom_target(class_parser_driver ${CLANG_HOME}/bin/clang++ -D_GNU_SOURCE -D_DEBUG -D__STDC_CONSTANT_MACROS
-D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_GNU_SOURCE
-I/Users/xaxxon/v8toolkit/tools//clang/include  -Iinclude
/Users/xaxxon/v8toolkit/tools/sample.cpp
-fsyntax-only -std=c++14 -O0
-Xclang -load
-Xclang /Users/xaxxon/Library/Caches/CLion2016.2/cmake/generated/v8toolkit-742e97ab/742e97ab/Debug/tools/libclass_parser.dylib
-Xclang -plugin
-Xclang v8toolkit-generate-bindings)

add_dependencies(class_parser_driver class_parser)




