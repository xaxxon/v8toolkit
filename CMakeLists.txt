cmake_minimum_required(VERSION 3.8)
set(CMAKE_VERBOSE_MAKEFILE on)

project(v8toolkit)
set (CMAKE_CXX_STANDARD 17)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_files)
include(ucm)
ucm_set_runtime(STATIC)


include(v8)

#message(looking for v8 libs in ${V8_LIB_DIR})
#FOREACH(LIB_NAME ${V8_LIB_NAMES})
#	FIND_LIBRARY(FOUND_LIB_${LIB_NAME} ${LIB_NAME} PATHS ${V8_LIB_DIR})
#	add_library(v8::${LIB_NAME} UNKNOWN IMPORTED)
#	set_target_properties(v8::${LIB_NAME} PROPERTIES
#			IMPORTED_LOCATION "${FOUND_LIB_${LIB_NAME}}"
#			INTERFACE_COMPILE_OPTIONS ""
#			INTERFACE_INCLUDE_DIRECTORIES ${V8_INCLUDE_DIR})
#
#	LIST(APPEND V8_LIBS v8::${LIB_NAME})
#	MESSAGE("Lib: v8::${LIB_NAME}")
#	MESSAGE("Found Lib: ${FOUND_LIB_${LIB_NAME}}")
#ENDFOREACH(LIB_NAME)


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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -g -Werror=return-type")
    set(V8_INCLUDE_DIR_DEFAULT ../v8/include)

endif()

# -DV8_BASE_SHARED_LIB_DIR:PATH=/path/to/v8/library/files
set(V8_LIB_DIR CACHE PATH "full path containing V8 library files")
#link_directories(${V8_BASE_SHARED_LIB_DIR})


# -DV8_INCLUDE_DIR:PATH=/path/to/v8/include/
set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR_DEFAULT} CACHE PATH "full path to v8 public headers directory containing v8.h")
include_directories(./include ${V8_INCLUDE_DIR})


# only use this against a shared-object-built V8
#   library=shared snapshot=off
add_library(v8toolkit_shared SHARED ${SOURCE_FILES})

# even when v8 is build shared, still needs some statically linked libs
target_link_libraries(v8toolkit_shared ${V8_LIBS})

# Linux doesn't need to link to this to make a shared object
target_link_libraries(v8toolkit_shared fmt ${Boost_LIBRARIES})

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
		COMMAND ${CMAKE_COMMAND} -E copy ${V8_BASE_SHARED_LIB_DIR}/*_blob.bin ${PROJECT_BINARY_DIR})


add_subdirectory(test)

add_subdirectory(class_parser)
