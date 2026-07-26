cmake_minimum_required(VERSION 3.10)
project(BessTests)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        52eb8108c5bdec04579160ae17225d66034bd723 # release-1.17.0
  FIND_PACKAGE_ARGS NAMES GTest
)
FetchContent_MakeAvailable(googletest)

add_executable(BessTests
    main_test.cpp
    project_session_test.cpp
    core_scene_commands_test.cpp
	    main_page_connection_commands_test.cpp
	    math_sim_driver_test.cpp
	    ui_layout_test.cpp
	    dock_manager_tests.cpp
	    input_event_tests.cpp
	    ui_target_event_tests.cpp
	    ui_action_tests.cpp
	    ui_drag_drop_tests.cpp
	    ui_visual_controls_tests.cpp
	    window_drop_service_test.cpp
	    ui_widget_tests.cpp
	    ui_tab_dock_tests.cpp
	    ui_menu_tests.cpp
	    ui_scroll_view_tests.cpp
	    wgpu_text_metrics_tests.cpp
	    ui_composition_tests.cpp
	    ui_focus_popup_controls_tests.cpp
	    ui_text_edit_tests.cpp
	    wgpu_renderer_batch_tests.cpp
    # scene_component_clone_test.cpp
    # sim_driver_test.cpp
    # scene_state_test.cpp
    # scene_serialization_test.cpp
    # simulation_engine_test.cpp
    # hierarchical_layout_test.cpp
    # verilog_import_test.cpp
    # connection_service_test.cpp
    # copy_paste_test.cpp
    # command_system_test.cpp
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_sources(BessTests PRIVATE x11_window_drop_handler_test.cpp)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/src/sim_engine/include"
	"${CMAKE_SOURCE_DIR}/src/bverilog/include"
	"${CMAKE_SOURCE_DIR}/src/plugin_manager/include"
	"${CMAKE_SOURCE_DIR}/src/bess/"
	"${CMAKE_SOURCE_DIR}/src/event_system/include"
	"${CMAKE_SOURCE_DIR}/src/common/include"
	"${CMAKE_SOURCE_DIR}/src/bess_core/include"
	"${CMAKE_SOURCE_DIR}/src/bess_wgpu/src"
	"${CMAKE_SOURCE_DIR}/src/math_sim_driver/include"
)

target_link_libraries(BessTests PRIVATE GTest::gtest BessRuntime)
include(GoogleTest)
gtest_discover_tests(BessTests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
