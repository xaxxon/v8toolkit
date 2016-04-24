cmake_minimum_required(VERSION 3.2)
project(v8toolkit)
set (CMAKE_CXX_STANDARD 14)

set(BOOST_INCLUDE_DIR c:\\boost\\include\\boost-1_60 CACHE PATH "root location of boost header files if not in /usr/local/include")

# This line unused because no compiled boost libraries are currently being used
#set(BOOST_LIB_DIR c:\\boost\\lib CACHE PATH "location of boost libraries if not in /usr/local/lib")



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

set(V8_LIBS
        v8_libplatform
        v8_libbase
        icui18n
        icuuc
        v8_nosnapshot
        v8_libplatforM)


if (WIN32)
    set(CPPFORMAT_BASE_DIR_DEFAULT c:\\program\ files\\FORMAT)
    set(V8_BASE_LIB_DIR_DEFAULT c:\\program\ files\\v8\\build\\Release\\lib)
    set(V8_INCLUDE_DIR_DEFAULT c:\\program\ files\\v8\\include)
    set(V8_LIBS ${V8_LIBS} v8_base_0 v8_base_1 v8_base_2 v8_base_3)


else()
    set(CPPFORMAT_BASE_DIR_DEFAULT /usr/local/)
    set(V8_BASE_LIB_DIR_DEFAULT ../v8/out/native)
    set(V8_INCLUDE_DIR_DEFAULT ../v8/include)
    set(V8_LIBS ${V8_LIBS} v8_base icudata)
endif()

set(CPPFORMAT_BASE_DIR ${CPPFORMAT_BASE_DIR_DEFAULT} CACHE PATH "path to root of cppformat that contains include and lib dirs")
set(V8_BASE_LIB_DIR ${V8_BASE_LIB_DIR_DEFAULT} CACHE PATH "full path including v8/out/<build_type>")
set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR_DEFAULT} CACHE PATH "full path to v8.h including 'include' at the end")


include_directories(. ${V8_INCLUDE_DIR_DEFAULT} ${BOOST_INCLUDE_DIR} ${CPPFORMAT_BASE_DIR}/include)

link_directories(${V8_BASE_LIB_DIR} ${V8_BASE_LIB_DIR}/obj.target/tools/gyp ${V8_BASE_LIB_DIR}/obj.target/third_party/icu)
link_directories(${CPPFORMAT_BASE_DIR}\\lib)


add_library(v8toolkit_shared SHARED ${SOURCE_FILES})
target_link_libraries(v8toolkit_shared ${V8_LIBS})
target_link_libraries(v8toolkit_shared cppformat)

set_target_properties(v8toolkit_shared PROPERTIES OUTPUT_NAME "v8toolkit")
add_library(v8toolkit_static STATIC ${SOURCE_FILES})
set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")



set(INSTALL_LIB_DIR     lib               CACHE PATH "Installation directory for libraries")
set(INSTALL_INCLUDE_DIR "include/v8toolkit" CACHE PATH "Installation directory for header files")

install(FILES ${HEADER_FILES} DESTINATION ${INSTALL_INCLUDE_DIR})
install(TARGETS v8toolkit_shared v8toolkit_static DESTINATION ${INSTALL_LIB_DIR})
