
include(ucm)

set(CMAKE_CXX_STANDARD 17)

set(CLANG_HOME $ENV{CLANG_HOME})

link_directories(${CLANG_HOME}/lib)

execute_process(COMMAND ${CLANG_HOME}/bin/llvm-config --ldflags --libs --system-libs
        OUTPUT_VARIABLE llvm_config_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )


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

FILE(GLOB CLASS_PARSER_SOURCES *.cpp *.h output_modules/*.h output_modules/*.cpp)

add_library(class_parser_plugin SHARED ${CLASS_PARSER_SOURCES})
target_include_directories(class_parser_plugin SYSTEM PRIVATE ${CLANG_HOME}/include)
        

# googletest sets the CMAKE_DEBUG_POSTFIX value to "d" so unset it here
set_target_properties(class_parser_plugin PROPERTIES
        DEBUG_POSTFIX "")

# No name mangling because no rtti because of clang
add_definitions(-DCLANG_HOME="${CLANG_HOME}" -DXL_FORCE_NO_DEMANGLE_NAMES)
include_directories(${gtest_SOURCE_DIR}/include)

# clang libs



target_link_libraries(class_parser_plugin fmt c++experimental pcre)


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

add_subdirectory(test EXCLUDE_FROM_ALL)