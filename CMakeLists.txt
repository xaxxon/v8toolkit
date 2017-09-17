cmake_minimum_required(VERSION 3.8)
set(CMAKE_VERBOSE_MAKEFILE on)

project(v8toolkit)
set (CMAKE_CXX_STANDARD 17)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)
include(ucm)
include(clang-build-triple)
ucm_set_runtime(STATIC)


include(v8)
link_directories(. ${CLANG_HOME}/lib)


file(GLOB HEADER_FILES
        include/*.h
		include/*.hpp
		include/debugger_message_types/*.h
		class_parser/class_parser.h
	)

file(GLOB SOURCE_FILES
        src/*.cpp
		src/debugger_message_types/*.cpp
        )




# https://github.com/fmtlib/fmt
#find_package(fmt REQUIRED)
include_directories(${fmt_INCLUDE_DIRS})

# https://github.com/zaphoyd/websocketpp
#find_package(websocketpp REQUIRED)
include_directories(${websocketpp_INCLUDE_DIRS})

# http://www.boost.org/
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIR})


if (WIN32)

else()


	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -g -Werror=return-type -target ${TARGET_TRIPLE}")
    set(V8_INCLUDE_DIR_DEFAULT ../v8/include)

endif()


set(V8_LIB_DIR CACHE PATH "full path containing V8 library files")


# -DV8_INCLUDE_DIR:PATH=/path/to/v8/include/
set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR_DEFAULT} CACHE PATH "full path to v8 public headers directory containing v8.h")
include_directories(./include ${V8_INCLUDE_DIR})


# only use this against a shared-object-built V8
#   library=shared snapshot=off
add_library(v8toolkit_shared SHARED ${SOURCE_FILES})

# even when v8 is build shared, still needs some statically linked libs
target_link_libraries(v8toolkit_shared ${V8_LIBS})

# make sure that libc++experimental.a from clang can be found.  May need to move it from clang distribution into /usr/lib
target_link_libraries(v8toolkit_shared fmt ${Boost_LIBRARIES} c++experimental)


add_library(v8toolkit_static STATIC ${SOURCE_FILES})
set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")
target_link_libraries(v8toolkit_static fmt ${Boost_LIBRARIES})

if (WIN32)
	target_link_libraries(v8toolkit_static WinMM)
endif()

set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_INCLUDE_DIR "include/v8toolkit" CACHE PATH "Installation directory for header files")

install(FILES ${HEADER_FILES} DESTINATION ${INSTALL_INCLUDE_DIR})
install(TARGETS v8toolkit_static v8toolkit_shared DESTINATION ${INSTALL_LIB_DIR})

link_directories(${PROJECT_BINARY_DIR})


add_custom_target(copy_snapshots)
add_custom_command(TARGET copy_snapshots PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy ${V8_LIB_DIR}/*_blob.bin ${PROJECT_BINARY_DIR})


add_subdirectory(test)

add_subdirectory(class_parser)
