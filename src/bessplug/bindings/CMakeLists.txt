cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME besspybindings)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

cmake_policy(SET CMP0148 NEW)

set(PYBIND11_FINDPYTHON ON)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)

include(FetchContent)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v3.0.0
)

FetchContent_Declare (
  pybind11_abseil
  GIT_REPOSITORY https://github.com/pybind/pybind11_abseil.git
  GIT_TAG        dba6e38b79d93bc718e28b8f8ea2e8b00b60cdec
)

set(BESS_BUILD_TESTING ${BUILD_TESTING})
set(BUILD_TESTING OFF)
FetchContent_MakeAvailable(pybind11 pybind11_abseil)
set(BUILD_TESTING ${BESS_BUILD_TESTING})
unset(BESS_BUILD_TESTING)

set(BESS_PYBIND_SOURCES
		internal_types.h

		bindings.cpp
		plugin.cpp
		common_bindings.cpp
		scene/renderer/path.binding.cpp
		scene/renderer/renderer.binding.cpp

		scene/schematic_diagram.binding.cpp
		scene/scene_state.binding.cpp
		scene/scene_common.binding.cpp
		scene/scene_component.binding.cpp
		scene/scene_ui.binding.cpp
		scene/sim_scene_component.binding.cpp
		scene/scene_widgets.binding.cpp

		sim_engine/types.binding.cpp
		sim_engine/sim_engine_core.binding.cpp
		sim_engine/sim_functions.binding.cpp
			sim_engine/driver/driver.binding.cpp
			sim_engine/driver/event_based_sim_driver.binding.cpp
			sim_engine/driver/dig_sim_driver.binding.cpp
			sim_engine/driver/math_sim_driver.binding.cpp
			verilog.binding.cpp

		assets/asset_manager.binding.cpp

		cmds/bess_cmds.binding.cpp
		
		core/icons.binding.cpp

		ui/ui_hook.binding.cpp
		ui/bess_ui.binding.cpp
		
)

pybind11_add_module(${PROJECT_NAME} ${BESS_PYBIND_SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE
	BessRuntime
	pybind11_abseil::absl_casters
)

add_dependencies(${PROJECT_NAME}
	BessRuntime
)

if(LINUX)
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

option(BESS_GENERATE_PY_STUBS "Run pybind11-stubgen after building bessplug" ON)

if(BESS_GENERATE_PY_STUBS)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import pybind11_stubgen"
        RESULT_VARIABLE _bess_stubgen_import_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    if(NOT _bess_stubgen_import_result EQUAL 0)
        message(FATAL_ERROR
            "BESS_GENERATE_PY_STUBS is ON but pybind11_stubgen is not available "
            "for Python \"${Python3_EXECUTABLE}\". Install it with:\n"
            "  \"${Python3_EXECUTABLE}\" -m pip install pybind11-stubgen\n"
            "Or configure with -DBESS_GENERATE_PY_STUBS=OFF.")
    endif()

    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND
            ${CMAKE_COMMAND}
            "-DPython3_EXECUTABLE=${Python3_EXECUTABLE}"
            "-DBESS_RUNTIME_DIR=$<TARGET_FILE_DIR:BessRuntime>"
            "-DBESS_MODULE_DIR=$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            "-DOUT_DIR=${OUT_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_stubgen.cmake"
        COMMENT "Generating python stubs for bessplug bindings..."
        VERBATIM
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    BUILD_RPATH "$<TARGET_FILE_DIR:BessRuntime>"
    INSTALL_RPATH "$ORIGIN"
)
