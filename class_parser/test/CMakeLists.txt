
add_subdirectory(googletest)

target_link_libraries(test-class-parser fmt ${GTEST_LIBRARIES})

add_dependencies(test-v8toolkit gmock gtest)