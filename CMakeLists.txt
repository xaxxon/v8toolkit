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


link_directories("../v8/out/native"
        ../v8/out/native/obj.target/tools/gyp
        ../v8/out/native/obj.target/third_party/icu)

set(V8_LIBS
        libv8_libplatform.a
        libv8_base.a
        libv8_libbase.a
        libicui18n.a
        libicuuc.a
        libicudata.a
        libv8_nosnapshot.a
        libv8_libplatform.a)

add_library(v8toolkit_shared SHARED ${SOURCE_FILES})
#target_link_libraries(v8toolkit_shared ${V8_LIBS})
      

set_target_properties(v8toolkit_shared PROPERTIES OUTPUT_NAME "v8toolkit")
add_library(v8toolkit_static STATIC ${SOURCE_FILES})
set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")


set(INSTALL_LIB_DIR     lib               CACHE PATH "Installation directory for libraries")
set(INSTALL_INCLUDE_DIR "include/v8toolkit" CACHE PATH "Installation directory for header files")

install(FILES ${HEADER_FILES} DESTINATION ${INSTALL_INCLUDE_DIR})
install(TARGETS v8toolkit_shared v8toolkit_static DESTINATION ${INSTALL_LIB_DIR})
