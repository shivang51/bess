cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME besspybindings)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

cmake_policy(SET CMP0148 NEW)

set(PYBIND11_FINDPYTHON ON)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)
find_package(pybind11 REQUIRED)

file(GLOB libtess2_src
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
	"${CMAKE_SOURCE_DIR}/external/imgui/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implot/*.cpp"
)

file(GLOB_RECURSE BESS_SOURCES
		"${CMAKE_SOURCE_DIR}/src/Bess/camera.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/pages/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/project_file.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/window.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/application/application_state.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/settings/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/common/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/events/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/settings/viewport_theme.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/asset_manager/*.cpp"
)

add_library(bess_ext_obj OBJECT
	${libtess2_src}
	${BESS_SOURCES}
)

set(EXTERNAL_INCLUDE_DIRS
			"${CMAKE_SOURCE_DIR}/external/imgui"
			"${CMAKE_SOURCE_DIR}/external/implot"
			"${CMAKE_SOURCE_DIR}/external/freetype/include"
			"${CMAKE_SOURCE_DIR}/external/imgui/backends"
			"${CMAKE_SOURCE_DIR}/external/glm"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/stb_image/include"
			"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/Bess/"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
			"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
)

target_include_directories(bess_ext_obj PRIVATE ${EXTERNAL_INCLUDE_DIRS})

set(BESS_PYBIND_SOURCES
		internal_types.h

    bindings.cpp
    common_bindings.cpp
    scene/renderer/path.cpp
    scene/renderer/path_renderer.binding.cpp
    scene/renderer/material_renderer.binding.cpp

		scene/schematic_diagram.cpp
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
	$<TARGET_OBJECTS:bess_ext_obj>
	BessVulkan
	BessSimEngine
	EventSystem
	BessScene
	BessUI
	"freetype"
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
	${EXTERNAL_INCLUDE_DIRS}
	"${CMAKE_CURRENT_SOURCE_DIR}/"
)


set(OUT_DIR "${CMAKE_SOURCE_DIR}/src/bessplug/bessplug/bindings/")

set_target_properties(${PROJECT_NAME} PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY "${OUT_DIR}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "_bindings"
)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env "PYTHONPATH=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            pybind11-stubgen 
            "_bindings"  # Use the actual OUTPUT_NAME/Module name here
            -o "${OUT_DIR}"
    WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Generating python stubs for bessplug_bindings..."
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
)
