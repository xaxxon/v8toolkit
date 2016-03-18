cmake_minimum_required(VERSION 3.3)
project(v8toolkit)

set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -std=c++14)

include_directories(. /Users/xaxxon/v8/include)

#link_directories(/Users/xaxxon/v8/out/native/)
link_directories(/Users/xaxxon/v8/out/x64.debug/)



set(SOURCE_FILES
#    samples/pure_v8_api_base_code_for_reproducing_issues.cpp
#    samples/sample.cpp
#    samples/thread_sample.cpp
#    samples/toolbox_sample.cpp
#    samples/toolbox_sample.js
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

set(V8_LIBS libv8_base.a libv8_libbase.a libicudata.a libicuuc.a libicui18n.a libv8_nosnapshot.a libv8_libplatform.a)

add_executable(bidirectional_sample ${SOURCE_FILES} samples/bidirectional_sample.cpp)
#add_executable(exception_sample ${SOURCE_FILES} samples/exception_sample.cpp)
#add_executable(javascript_sample ${SOURCE_FILES} samples/javascript_sample.cpp)

target_link_libraries (bidirectional_sample ${V8_LIBS})
#target_link_libraries (exception_sample     ${V8_LIBS})
#target_link_libraries (javascript_sample    ${V8_LIBS})
