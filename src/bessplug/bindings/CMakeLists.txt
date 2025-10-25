cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME besspybindings)
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

cmake_policy(SET CMP0148 NEW)

set(PYBIND11_FINDPYTHON ON)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)
find_package(pybind11 REQUIRED)

file(GLOB libtess2_src
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
)

file(GLOB_RECURSE BESS_SCENE_SOURCES
		# "${CMAKE_SOURCE_DIR}/src/Bess/src/scene/renderer/vulkan/path_renderer.cpp"
		# "${CMAKE_SOURCE_DIR}/src/Bess/src/scene/renderer/vulkan/pipelines/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/src/scene/renderer/*.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/src/camera.cpp"
		"${CMAKE_SOURCE_DIR}/src/Bess/src/asset_manager/*.cpp"
)

add_library(bess_scene_obj OBJECT
	${BESS_SCENE_SOURCES}
	${libtess2_src}
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
			"${CMAKE_SOURCE_DIR}/external/entt/include"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/Bess/include"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
)

target_include_directories(bess_scene_obj PRIVATE ${EXTERNAL_INCLUDE_DIRS})

set(BESS_PYBIND_SOURCES
		internal_types.h

    bindings.cpp
    common_bindings.cpp
    sim_engine/types.cpp
    renderer/path.cpp
    sim_engine/component_definition.cpp
)

set(BESS_PYBIND_SOURCES ${BESS_PYBIND_SOURCES})

pybind11_add_module(${PROJECT_NAME} ${BESS_PYBIND_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
	$<TARGET_OBJECTS:bess_scene_obj>
	BessVulkan
	BessSimEngine
	"freetype"
)

target_include_directories(${PROJECT_NAME} PRIVATE 
	${EXTERNAL_INCLUDE_DIRS}
	"${CMAKE_CURRENT_SOURCE_DIR}/"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/src/bessplug/bessplug/bindings"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "_bindings"
)

add_dependencies(${PROJECT_NAME} BessSimEngine)

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
)
