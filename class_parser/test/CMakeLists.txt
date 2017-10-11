project ("Class Parser Test")

include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR}/include)


add_executable(test-class-parser test.cpp ${CLASS_PARSER_SOURCES})

target_link_libraries(test-class-parser gmock gmock_main)

add_dependencies(test-v8toolkit gmock gtest)

target_link_libraries(test-class-parser clangFrontend clangSerialization clangASTMatchers clangSema
        clangAnalysis clangRewriteFrontend clangEdit clangDriver clangLex
        LLVMSymbolize LLVMDebugInfoPDB LLVMScalarOpts LLVMInstCombine LLVMInstrumentation LLVMBitWriter
        LLVMTransformUtils LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMRuntimeDyld
        LLVMObject LLVMBitReader LLVMCore clangParse clangAST
        clangBasic clangParse clangTooling)
