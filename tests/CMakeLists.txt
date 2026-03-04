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

add_executable(BessTests main.cpp scene_state_test.cpp connection_service_test.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/src/sim_engine/include"
	"${CMAKE_SOURCE_DIR}/src/bess/"
	"${CMAKE_SOURCE_DIR}/src/event_system/include"
	"${CMAKE_SOURCE_DIR}/src/common/include"
)

target_link_libraries(BessTests PRIVATE GTest::gtest_main BessExtDeps common BessSimEngine BessScene BessUI BessApp EventSystem)

include(GoogleTest)
gtest_discover_tests(BessTests)
