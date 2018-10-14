
project ("v8toolkit Test")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftemplate-backtrace-limit=0")


set(CMAKE_VERBOSE_MAKEFILE on)


include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR}/include)

file(GLOB TEST_SRC
        *.cpp
        )

add_definitions(-DGOOGLE_TEST)

set(CLANG_HOME $ENV{CLANG_HOME})
link_directories(. ${CLANG_HOME}/lib)


add_executable(test-v8toolkit ${TEST_SRC})
add_dependencies(test-v8toolkit copy_snapshots v8toolkit_shared)


add_dependencies(test-v8toolkit gmock gtest)
message("${MY_SRC}")

# Causing false positives on vector re-allocations
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

#SdlStaticLibs(test-apb)

target_link_libraries(test-v8toolkit v8toolkit_shared fmt gmock gmock_main ${V8_SHARED_LIBS} boost_system EASTL )
add_custom_target(copy_javascript_sources)
add_custom_command(TARGET copy_javascript_sources PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/*.js ${PROJECT_BINARY_DIR})


add_dependencies(test-v8toolkit copy_javascript_sources)