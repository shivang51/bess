cmake_minimum_required(VERSION 3.16.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)

project(bess C CXX)

option(TESTS "Build the unit tests" OFF)
option(DOCS  "Generate documentation with Doxygen" OFF)

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

if(TESTS)
	message(STATUS "Tests enabled")
	include(CTest)
	enable_testing()
endif()

if(DOCS)
	message(STATUS "Doxygen docs enabled")
endif()


add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

# add_compile_definitions(DISABLE_PLUGINS)

add_subdirectory(external)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

if(DOCS)
		find_package(Doxygen)
		if(DOXYGEN_FOUND)
				add_subdirectory(docs)
		else()
				message(WARNING "Doxygen not found, skipping documentation generation")
		endif()
endif()

if(MSVC)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT bess)
endif()
