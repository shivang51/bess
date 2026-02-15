cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME besspybindings)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

cmake_policy(SET CMP0148 NEW)

set(PYBIND11_FINDPYTHON ON)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)
find_package(pybind11 REQUIRED)

set(BESS_PYBIND_SOURCES
		internal_types.h

    bindings.cpp
		plugin.cpp
    common_bindings.cpp
    scene/renderer/path.binding.cpp
    scene/renderer/path_renderer.binding.cpp
    scene/renderer/material_renderer.binding.cpp

		scene/schematic_diagram.binding.cpp
		scene/scene_state.binding.cpp
		scene/scene_common.binding.cpp
		scene/scene_component.binding.cpp
		scene/sim_comp_draw_hook.binding.cpp

    sim_engine/types.cpp
    sim_engine/sim_functions.cpp
    sim_engine/component_definition.binding.cpp

		assets/asset_manager.binding.cpp

		ui/ui_hook.binding.cpp
)

pybind11_add_module(${PROJECT_NAME} ${BESS_PYBIND_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
	BessExtDeps
	BessVulkan
	BessSimEngine
	EventSystem
	BessScene
	BessUI
	BessJson
	BessCmdSystem
	BessApp
)

add_dependencies(${PROJECT_NAME}
	BessApp
	BessExtDeps
	BessVulkan
	BessSimEngine
	EventSystem
	BessScene
	BessUI
	BessJson
)

find_package(glfw3 REQUIRED)
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    find_package(Threads REQUIRED)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        glfw 
        X11::X11 
        Threads::Threads
        ${CMAKE_DL_LIBS}
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/"
)


set(OUT_DIR "${CMAKE_SOURCE_DIR}/src/bessplug/py/")

set_target_properties(${PROJECT_NAME} PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "bessplug"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            pybind11-stubgen 
            "bessplug"
            -o "${OUT_DIR}"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Generating python stubs for bessplug bindings..."
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
)
