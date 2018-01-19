cmake_minimum_required(VERSION 3.8)
set(CMAKE_VERBOSE_MAKEFILE on)

project(v8toolkit)
set (CMAKE_CXX_STANDARD 17)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake-modules)
include(ucm)
include(clang-build-triple)
ucm_set_runtime(STATIC)

set(V8TOOLKIT_VERSION 0.1.0)

include(v8)

set(CLANG_HOME $ENV{CLANG_HOME})
link_directories(. ${CLANG_HOME}/lib)


file(GLOB HEADER_FILES
        include/*.h
		include/*.hpp
		include/debugger_message_types/*.h
		class_parser/class_parser.h)

file(GLOB SOURCE_FILES
        src/*.cpp
		src/debugger_message_types/*.cpp)


# https://github.com/fmtlib/fmt
find_package(FMT REQUIRED)

# https://github.com/zaphoyd/websocketpp
find_package(websocketpp REQUIRED)

# http://www.boost.org/
find_package(Boost REQUIRED COMPONENTS system)

# bring in libv8toolkit and make require that it have access to libfmt for additional logging functionality
set(v8toolkit::USE_LIB_FMT REQUIRED)
find_package(xl REQUIRED)



if(WIN32)

else()
	# -fsanitize=undefined -fno-sanitize=vptr
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -stdlib=libc++ -g -Werror=return-type -Wall -target ${TARGET_TRIPLE}")
    set(V8_INCLUDE_DIR_DEFAULT ../v8/include)
endif()


set(V8_LIB_DIR CACHE PATH "full path containing V8 library files")


# -DV8_INCLUDE_DIR:PATH=/path/to/v8/include/
set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR_DEFAULT} CACHE PATH "full path to v8 public headers directory containing v8.h")
include_directories(./include ${V8_INCLUDE_DIR})


# only use this against a shared-object-built V8
#   library=shared snapshot=off
add_library(v8toolkit_shared SHARED ${SOURCE_FILES})
add_library(v8toolkit::v8toolkit_shared ALIAS v8toolkit_shared)

# make sure that libc++experimental.a from clang can be found.  May need to move it from clang distribution into /usr/lib
target_link_libraries(v8toolkit_shared xl::xl ${V8_LIBS} ${Boost_LIBRARIES} c++experimental fmt ${Boost_LIBRARIES} websocketpp::websocketpp)


add_library(v8toolkit_static STATIC ${SOURCE_FILES})
add_library(v8toolkit::v8toolkit_static ALIAS v8toolkit_static)

set_target_properties(v8toolkit_static PROPERTIES OUTPUT_NAME "v8toolkit")
target_link_libraries(v8toolkit_static fmt ${Boost_LIBRARIES})

if(WIN32)
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

add_subdirectory(googletest EXCLUDE_FROM_ALL)
add_subdirectory(test EXCLUDE_FROM_ALL)

# turn off adding "d" suffix to debug versions of libraries
set_target_properties(v8toolkit_shared PROPERTIES
        DEBUG_POSTFIX "d")
set_target_properties(v8toolkit_static PROPERTIES
		DEBUG_POSTFIX "d")

	
add_subdirectory(class_parser)





set(INCLUDE_INSTALL_DIR "include/" CACHE PATH "Installation directory for header files")

install(TARGETS v8toolkit_static v8toolkit_shared
		EXPORT V8TOOLKIT_TARGETS
		LIBRARY  DESTINATION lib
		ARCHIVE  DESTINATION lib
		RUNTIME  DESTINATION bin
		INCLUDES DESTINATION include)

install(DIRECTORY include/v8toolkit DESTINATION ${INCLUDE_INSTALL_DIR})
export(EXPORT V8TOOLKIT_TARGETS
		FILE "${CMAKE_CURRENT_BINARY_DIR}/v8toolkitTargets.cmake"
		NAMESPACE v8toolkit::
		)

set(CONFIG_PACKAGE_LOCATION lib/cmake/v8toolkit)

# This complains about "INTERFACE_SOURCES property contains path "..." which is prefixed in the source directory"
install(EXPORT V8TOOLKIT_TARGETS
		FILE
		v8toolkitTargets.cmake
		NAMESPACE
		v8toolkit::
		DESTINATION
		${CONFIG_PACKAGE_LOCATION}
		)



include(CMakePackageConfigHelpers)
write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/v8toolkitConfigVersion.cmake"
		VERSION $V8TOOLKIT_VERSION
		COMPATIBILITY SameMajorVersion
)

set(CONFIG_PACKAGE_LOCATION lib/cmake/v8toolkit)

configure_package_config_file(v8toolkitConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/v8toolkitConfig.cmake
		INSTALL_DESTINATION ${CONFIG_PACKAGE_LOCATION}
		PATH_VARS INCLUDE_INSTALL_DIR V8TOOLKIT_VERSION)


install(FILES ${CMAKE_CURRENT_BINARY_DIR}/v8toolkitConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/v8toolkitConfigVersion.cmake
		DESTINATION ${CONFIG_PACKAGE_LOCATION})
