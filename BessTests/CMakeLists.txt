cmake_minimum_required(VERSION 3.10)
project(BessTests)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

FetchContent_MakeAvailable(googletest)

add_executable(BessTests main.cpp)



target_link_libraries(BessTests PRIVATE GTest::gtest_main BessSimEngine)

include(GoogleTest)
gtest_discover_tests(BessTests)
