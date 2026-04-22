cmake_minimum_required(VERSION 3.10)
project(BessTests)

set(CMAKE_CXX_STANDARD 23)

find_package(GTest CONFIG REQUIRED)

add_executable(BessTests
    main_test.cpp
    sim_driver_test.cpp
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

target_link_libraries(BessTests PRIVATE GTest::gtest BessExtDeps common BessSimEngine bverilog BessScene BessUI BessApp EventSystem)

include(GoogleTest)
gtest_discover_tests(BessTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
