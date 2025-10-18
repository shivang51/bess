cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME besspybindings)
project(${PROJECT_NAME} LANGUAGES CXX)

cmake_policy(SET CMP0148 NEW)

set(PYBIND11_FINDPYTHON ON)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)
find_package(pybind11 REQUIRED)

set(BESS_PYBIND_SOURCES
    bindings.cpp
    sim_engine/types.cpp
    sim_engine/component_definition.cpp
)

pybind11_add_module(${PROJECT_NAME} ${BESS_PYBIND_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/BessSimEngine/include"
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
    "${CMAKE_SOURCE_DIR}/external/entt/include;"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include;"
    "${CMAKE_SOURCE_DIR}/BessPluginManager/include;"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bessplug/bessplug/bindings"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "_bindings"
)

add_dependencies(${PROJECT_NAME} BessSimEngine)

# Link against the simulation engine to resolve symbols at import time
target_link_libraries(${PROJECT_NAME} PRIVATE BessSimEngine)

# Ensure the loader can find dependent shared libs from the build tree
set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
    INSTALL_RPATH "${CMAKE_SOURCE_DIR}/build/lib"
)
