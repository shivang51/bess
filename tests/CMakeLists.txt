cmake_minimum_required(VERSION 3.10)
project(BessTests)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

add_executable(BessTests main.cpp)

if("${BUILD_ARCH}" STREQUAL "x64")
	target_include_directories(${PROJECT_NAME} PRIVATE
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/entt/include"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include;"
	)
endif()


target_link_libraries(BessTests PRIVATE GTest::gtest_main BessSimEngine)

include(GoogleTest)
gtest_discover_tests(BessTests)
