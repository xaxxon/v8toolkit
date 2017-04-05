
project ("v8toolkit Test")

include_directories(${gtest_SOURCE_DIR}/include)
include_directories(${gmock_SOURCE_DIR}/include)

file(GLOB TEST_SRC
        *.cpp
        )

add_definitions(-DGOOGLE_TEST )

add_subdirectory(googletest)

add_executable(test-v8toolkit ${TEST_SRC})

add_dependencies(test-v8toolkit gmock gtest)
message("${MY_SRC}")

#SdlStaticLibs(test-apb)

set(CMAKE_CXX_STANDARD 14)

target_link_libraries(test-v8toolkit v8toolkit_shared fmt gmock gmock_main ${V8_SHARED_LIBS} boost_system EASTL )


