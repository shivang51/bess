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

set(External
		"${CMAKE_SOURCE_DIR}/src/Bess/settings/viewport_theme.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/settings/themes.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/settings/settings.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/pages/page.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/pages/main_page/main_page_state.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/pages/main_page/main_page.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/pages/main_page/scene_driver.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/common/helpers.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/application_state.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/application.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/window.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/project_file.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/events/application_event.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/asset_manager/asset_manager.cpp"
)

add_executable(BessTests ${External} main.cpp scene_state_test.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
	"${CMAKE_SOURCE_DIR}/src/Bess/"
	"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
)

target_link_libraries(BessTests PRIVATE GTest::gtest_main BessExtDeps BessSimEngine BessScene BessUI EventSystem)

include(GoogleTest)
gtest_discover_tests(BessTests)
