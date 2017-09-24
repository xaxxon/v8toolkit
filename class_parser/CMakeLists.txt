
include(cotire)
include(ucm)

set(CMAKE_CXX_STANDARD 17)

set(CLANG_HOME $ENV{CLANG_HOME})

link_directories(${CLANG_HOME}/lib)
include_directories(${V8_INCLUDE_DIR})
include_directories(${CLANG_HOME}/include)

execute_process(COMMAND ${CLANG_HOME}/bin/llvm-config --ldflags --libs --system-libs
        OUTPUT_VARIABLE llvm_config_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )

find_package(gtest REQUIRED)

string(REPLACE "\n" " " llvm_config_output ${llvm_config_output})

if(UNIX AND NOT APPLE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow -stdlib=libstdc++")
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow -stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -pedantic")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${llvm_config_output}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${llvm_config_output}")

set(CLASS_PARSER_SOURCES parsed_method.cpp class_parser.cpp ast_action.cpp annotations.cpp helper_functions.cpp
        annotations.cpp class_handler.cpp ast_consumer.cpp wrapped_class.cpp)

add_library(class_parser_plugin SHARED ${CLASS_PARSER_SOURCES})
add_definitions(-DCLANG_HOME="${CLANG_HOME}")
include_directories(${gtest_SOURCE_DIR}/include)
add_executable(test-class-parser ${CLASS_PARSER_SOURCES} test/test.cpp)
cotire(test-class-parser)

# clang libs
target_link_libraries(test-class-parser clangFrontend clangSerialization clangASTMatchers clangSema
        clangAnalysis clangRewriteFrontend clangEdit clangDriver clangLex
        LLVMSymbolize LLVMDebugInfoPDB LLVMScalarOpts LLVMInstCombine LLVMInstrumentation LLVMBitWriter
        LLVMTransformUtils LLVMExecutionEngine LLVMTarget LLVMAnalysis LLVMRuntimeDyld
        LLVMObject LLVMBitReader LLVMCore clangParse clangAST
        clangBasic clangParse clangTooling)

target_link_libraries(test-class-parser fmt ${GTEST_LIBRARIES})

cotire(class_parser_plugin)

target_link_libraries(class_parser_plugin fmt)


add_custom_target(class_parser_driver
        ${CLANG_HOME}/bin/clang++ -D_GNU_SOURCE -D_DEBUG -D__STDC_CONSTANT_MACROS
        -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS -D_GNU_SOURCE
        ${PROJECT_BINARY_DIR}/class_parser/sample.cpp
        -fsyntax-only -std=c++1z -O0
        -Xclang -load
        -Xclang ${CMAKE_BINARY_DIR}/class_parser/$<TARGET_FILE_NAME:class_parser_plugin>
        -Xclang -plugin
        -Xclang v8toolkit-generate-bindings
        -Xclang -plugin-arg-v8toolkit-generate-bindings
        -Xclang --declaration-count=1000
    )

add_custom_target(copy_driver_resources)
add_custom_command(TARGET copy_driver_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/sample.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/sample2.h
            ${CMAKE_CURRENT_SOURCE_DIR}/class_parser.h
        ${PROJECT_BINARY_DIR}/class_parser/)

add_dependencies(class_parser_driver class_parser_plugin copy_driver_resources)

install(TARGETS class_parser_plugin DESTINATION ${INSTALL_LIB_DIR})

