cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(bess C CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
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

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/${BUILD_ARCH}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/${BUILD_ARCH}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}/${BUILD_ARCH}/static")

message(STATUS "Output location: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_subdirectory(BessSimEngine)
add_subdirectory(Bess)
add_subdirectory(BessTests)

if(MSVC)
	# startup projects needs to be set here
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Bess)
endif()
