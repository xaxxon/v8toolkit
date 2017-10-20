project ("Class Parser Test")
set(CMAKE_VERBOSE_MAKEFILE on)
include(cotire)


include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR}/include)


add_executable(test-class-parser test.cpp ${CLASS_PARSER_SOURCES})
find_package(fmt REQUIRED)
target_link_libraries(test-class-parser fmt gmock gmock_main)

# Can't have RTTI because the clang libs don't have it which means
target_compile_options(test-class-parser PRIVATE -fno-rtti)

target_link_libraries(test-class-parser clangFrontend clangSerialization clangASTMatchers clangSema
        clangAnalysis clangRewriteFrontend clangEdit clangDriver clangLex
        LLVMSymbolize LLVMDebugInfoPDB LLVMScalarOpts LLVMInstCombine LLVMInstrumentation LLVMBitWriter
        LLVMTransformUtils LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMRuntimeDyld
        LLVMObject LLVMBitReader LLVMCore clangParse clangAST
        clangBasic clangParse clangTooling c++experimental pcre)


add_custom_target(copy_test_resources)
add_custom_command(TARGET copy_test_resources PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/../class_parser.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/wrapped_class_base.h
        ${PROJECT_BINARY_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../output_modules/javascript_stub_templates/
        ${PROJECT_BINARY_DIR}/javascript_stub_templates/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../output_modules/bindings_templates/
        ${PROJECT_BINARY_DIR}/bindings_templates/
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../output_modules/bidirectional_templates/
        ${PROJECT_BINARY_DIR}/bidirectional_templates/
        )

add_dependencies(test-class-parser gmock gtest copy_test_resources)


cotire(test-class-parser)
add_dependencies(test-class-parser_unity gmock gtest copy_test_resources)
