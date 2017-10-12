project ("Class Parser Test")
set(CMAKE_VERBOSE_MAKEFILE on)


include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR}/include)


add_executable(test-class-parser test.cpp ${CLASS_PARSER_SOURCES})
find_package(fmt REQUIRED)
target_link_libraries(test-class-parser fmt gmock gmock_main)

add_dependencies(test-class-parser gmock gtest copy_test_resources)

target_link_libraries(test-class-parser clangFrontend clangSerialization clangASTMatchers clangSema
        clangAnalysis clangRewriteFrontend clangEdit clangDriver clangLex
        LLVMSymbolize LLVMDebugInfoPDB LLVMScalarOpts LLVMInstCombine LLVMInstrumentation LLVMBitWriter
        LLVMTransformUtils LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMRuntimeDyld
        LLVMObject LLVMBitReader LLVMCore clangParse clangAST
        clangBasic clangParse clangTooling)



add_custom_target(copy_test_resources)
add_custom_command(TARGET copy_test_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/class_parser_input_files/sample.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/class_parser_input_files/sample.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../class_parser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/wrapped_class_base.h
        ${PROJECT_BINARY_DIR}/)
