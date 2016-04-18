cmake_minimum_required(VERSION 3.2)
project(v8toolkit)
set (CMAKE_CXX_STANDARD 14)

include_directories(. ../v8/include)

set(SOURCE_FILES
        bidirectional.h
        casts.hpp
        casts_impl.hpp
        javascript.cpp
        javascript.h
        v8_class_wrapper.h
        v8helpers.cpp
        v8helpers.h
        v8toolkit.cpp
        v8toolkit.h)


add_library(v8toolkit_shared SHARED ${SOURCE_FILES})
set_target_properties(v8toolkit_shared PROPERTIES OUTPUT_NAME "v8toolkit")
add_library(v8toolkit_static STATIC ${SOURCE_FILES})
set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")
