cmake_minimum_required(VERSION 3.2)
project(v8toolkit)
set (CMAKE_CXX_STANDARD 14)

include_directories(. ../v8/include)

set(HEADER_FILES
        bidirectional.h
        casts.hpp
        casts_impl.hpp
        javascript.h
        v8_class_wrapper.h
        v8helpers.h
        v8toolkit.h)

set(SOURCE_FILES
        casts.hpp
        javascript.cpp
        v8helpers.cpp
        v8toolkit.cpp)

add_library(v8toolkit_shared SHARED ${SOURCE_FILES})
set_target_properties(v8toolkit_shared PROPERTIES OUTPUT_NAME "v8toolkit")
add_library(v8toolkit_static STATIC ${SOURCE_FILES})
set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")


set(INSTALL_LIB_DIR     lib               CACHE PATH "Installation directory for libraries")
set(INSTALL_INCLUDE_DIR "include/v8toolkit" CACHE PATH "Installation directory for header files")

install(FILES ${HEADER_FILES} DESTINATION ${INSTALL_INCLUDE_DIR})
install(TARGETS v8toolkit_shared v8toolkit_static DESTINATION ${INSTALL_LIB_DIR})
