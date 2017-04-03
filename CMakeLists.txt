cmake_minimum_required(VERSION 3.5)


project(v8toolkit)
set (CMAKE_CXX_STANDARD 14)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_files)
include(ucm)
ucm_set_runtime(STATIC)


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

set(V8_STATIC_LIBS
        v8_libplatform
        v8_libbase
        icui18n
        icuuc
        v8_nosnapshot
        v8_libplatform)

set(V8_SHARED_LIBS
	icui18n
	icuuc 
	v8
	v8_libbase
		v8_libplatform)

# https://github.com/fmtlib/fmt
find_package(fmt REQUIRED)
include_directories(${fmt_INCLUDE_DIRS})

# https://github.com/zaphoyd/websocketpp
find_package(websocketpp REQUIRED)
include_directories(${websocketpp_INCLUDE_DIRS})

# http://www.boost.org/
find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIR})


if (WIN32)
    set(V8_STATIC_LIBS ${V8_LIBS} v8_base_0 v8_base_1 v8_base_2 v8_base_3)

else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -g")
    set(V8_INCLUDE_DIR_DEFAULT ../v8/include)
    set(V8_STATIC_LIBS ${V8_STATIC_LIBS} v8_base icudata)

endif()

# -DV8_BASE_SHARED_LIB_DIR:PATH=/path/to/v8/library/files
set(V8_BASE_SHARED_LIB_DIR CACHE PATH "full path containing V8 library files")
link_directories(${V8_BASE_SHARED_LIB_DIR})


# -DV8_INCLUDE_DIR:PATH=/path/to/v8/include/
set(V8_INCLUDE_DIR ${V8_INCLUDE_DIR_DEFAULT} CACHE PATH "full path to v8 public headers directory containing v8.h")
include_directories(./include ${V8_INCLUDE_DIR})


# only use this against a shared-object-built V8
#   library=shared snapshot=off
add_library(v8toolkit_shared SHARED ${SOURCE_FILES})

# even when v8 is build shared, still needs some statically linked libs
target_link_libraries(v8toolkit_shared ${V8_SHARED_LIBS})

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

add_executable(bidirectional_sample samples/bidirectional_sample.cpp)
#add_dependencies(bidirectional_sample v8toolkit_static copy_bidirectional_sample_resources)
add_dependencies(bidirectional_sample v8toolkit_shared copy_bidirectional_sample_resources)
#target_link_libraries(bidirectional_sample ${V8_STATIC_LIBS})
target_link_libraries(bidirectional_sample ${V8_SHARED_LIBS})
target_link_libraries(bidirectional_sample v8toolkit_shared)
target_link_libraries(bidirectional_sample fmt)

add_custom_target(copy_bidirectional_sample_resources)
add_custom_command(TARGET copy_bidirectional_sample_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/samples/bidirectional.js ${PROJECT_BINARY_DIR})

add_executable(exception_sample samples/exception_sample.cpp)
add_dependencies(exception_sample v8toolkit_shared)
#target_link_libraries(exception_sample ${V8_STATIC_LIBS})
target_link_libraries(exception_sample ${V8_SHARED_LIBS})
target_link_libraries(exception_sample v8toolkit_shared fmt)
#add_custom_command(TARGET bidirectional_sample PRE_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/samples/bidirectional.js
#        ${PROJECT_BINARY_DIR})


add_executable(thread_sample samples/thread_sample.cpp)
add_dependencies(thread_sample v8toolkit_shared)
target_link_libraries(thread_sample ${V8_SHARED_LIBS})
#target_link_libraries(thread_sample ${V8_STATIC_LIBS})
target_link_libraries(thread_sample v8toolkit_shared fmt)

add_executable(toolbox_sample samples/toolbox_sample.cpp)
add_dependencies(toolbox_sample v8toolkit_shared copy_toolbox_sample_resources)
target_link_libraries(toolbox_sample ${V8_SHARED_LIBS})
#target_link_libraries(toolbox_sample ${V8_STATIC_LIBS})
target_link_libraries(toolbox_sample v8toolkit_shared fmt)

add_custom_target(copy_toolbox_sample_resources)
add_custom_command(TARGET copy_toolbox_sample_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/toolbox_sample.js
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/module.js
        ${CMAKE_CURRENT_SOURCE_DIR}/samples/module2.js
        ${PROJECT_BINARY_DIR})

add_executable(javascript_sample samples/javascript_sample.cpp)
add_dependencies(javascript_sample v8toolkit_shared)
#target_link_libraries(javascript_sample ${V8_STATIC_LIBS})
target_link_libraries(javascript_sample ${V8_SHARED_LIBS})
target_link_libraries(javascript_sample v8toolkit_shared fmt pthread)


add_executable(sample samples/sample.cpp)
add_dependencies(sample v8toolkit_shared copy_sample_resources)
target_link_libraries(sample ${V8_SHARED_LIBS})
#target_link_libraries(sample ${V8_STATIC_LIBS})
target_link_libraries(sample v8toolkit_shared fmt EASTL)

add_custom_target(copy_sample_resources)
add_custom_command(TARGET copy_sample_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/samples/code.js
        ${PROJECT_BINARY_DIR})


# DEBUGGER SAMPLE
add_executable(debugger_sample samples/debugger_sample.cpp)
add_dependencies(debugger_sample v8toolkit_shared copy_debugger_resources)
target_link_libraries(debugger_sample ${V8_SHARED_LIBS} v8toolkit_shared fmt)
add_custom_target(copy_debugger_resources)
add_custom_command(TARGET copy_debugger_resources POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/samples/debugger_sample.js
        ${PROJECT_BINARY_DIR})


add_custom_target(samples)
add_dependencies(samples bidirectional_sample thread_sample exception_sample toolbox_sample)

add_custom_target(run_samples)
add_dependencies(run_samples samples)
add_custom_command(TARGET run_samples POST_BUILD
        COMMAND ${PROJECT_BINARY_DIR}/bidirectional_sample && ${PROJECT_BINARY_DIR}/thread_sample && ${PROJECT_BINARY_DIR}/exception_sample && ${PROJECT_BINARY_DIR}/toolbox_sample)


add_subdirectory(class_parser)
