cmake_minimum_required(VERSION 3.10)
project(BessTests)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

find_package(GTest CONFIG QUIET)

if(NOT GTest_FOUND AND BESS_FETCH_DEPS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

if(TARGET GTest::gtest)
    set(BESS_GTEST_TARGET GTest::gtest)
elseif(TARGET gtest)
    set(BESS_GTEST_TARGET gtest)
else()
    message(FATAL_ERROR
        "GTest was not found. Install GTest or configure with -DBESS_FETCH_DEPS=ON to fetch it automatically.")
endif()

add_executable(BessTests
    main.cpp
    scene_state_test.cpp
    scene_serialization_test.cpp
    scene_component_clone_test.cpp
    simulation_engine_test.cpp
    hierarchical_layout_test.cpp
    verilog_import_test.cpp
    connection_service_test.cpp
    copy_paste_test.cpp
    command_system_test.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/src/sim_engine/include"
	"${CMAKE_SOURCE_DIR}/src/bverilog/include"
	"${CMAKE_SOURCE_DIR}/src/plugin_manager/include"
	"${CMAKE_SOURCE_DIR}/src/command_system/include"
	"${CMAKE_SOURCE_DIR}/src/bess/"
	"${CMAKE_SOURCE_DIR}/src/event_system/include"
	"${CMAKE_SOURCE_DIR}/src/common/include"
)

target_link_libraries(BessTests PRIVATE ${BESS_GTEST_TARGET} BessExtDeps common BessSimEngine bverilog BessScene BessUI BessApp EventSystem)

include(GoogleTest)
gtest_discover_tests(BessTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
