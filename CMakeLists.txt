cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

project(bess C CXX)
include(CTest)
enable_testing()
set(CMAKE_CXX_STANDARD 23)

if(WIN32 AND CMAKE_GENERATOR MATCHES "Ninja")
	# build_debug scripts run an explicit configure step, so disabling auto-regeneration
	# avoids pathological Ninja reconfigure loops in some Windows environments.
	set(CMAKE_SUPPRESS_REGENERATION ON)
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug)
endif()

message(STATUS "Building in ${CMAKE_BUILD_TYPE} mode")

# I know person building the project will know his arch and os.
# These are for me so I don't forget how to query them :)
message(STATUS "OS: ${CMAKE_SYSTEM_NAME}")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(BUILD_ARCH "x64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(BUILD_ARCH "x32")
endif()

message(STATUS "Arch: ${BUILD_ARCH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>/${BUILD_ARCH}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>/${BUILD_ARCH}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>/${BUILD_ARCH}/static")

message(STATUS "Output location: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

# add_compile_definitions(DISABLE_PLUGINS)

include(cmake/Dependencies.cmake)

add_subdirectory(external)
add_subdirectory(src)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
# add_subdirectory(docs)

if(MSVC)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Bess)
endif()
