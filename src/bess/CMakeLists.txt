set(PROJECT_NAME Bess)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_BUILD_RPATH "\$ORIGIN")
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory("application")

include(GenerateExportHeader)

set(BESS_RUNTIME_COMPONENT_TARGETS
    common
    BessJson
    bverilog
    BessPluginManager
    EventSystem
    BessSimEngine
    BessSimDriver
    DigitalSimDriver
    MathSimDriver
    ProjectSession
    BessCore
    BessWgpu
    BessApp
    BessExtDeps
		UICore
)

set(BESS_RUNTIME_GENERATED_INCLUDE_DIR
    "${CMAKE_BINARY_DIR}/generated/bess_runtime"
)
file(MAKE_DIRECTORY "${BESS_RUNTIME_GENERATED_INCLUDE_DIR}")

add_library(BessRuntime SHARED "runtime.cpp")
generate_export_header(
    BessRuntime
    BASE_NAME BESS_RUNTIME
    EXPORT_MACRO_NAME BESS_RUNTIME_API
    EXPORT_FILE_NAME
        "${BESS_RUNTIME_GENERATED_INCLUDE_DIR}/bess_runtime_export.h"
    STATIC_DEFINE BESS_RUNTIME_STATIC
)

target_include_directories(BessRuntime PUBLIC
    "$<BUILD_INTERFACE:${BESS_RUNTIME_GENERATED_INCLUDE_DIR}>"
)

target_link_libraries(BessRuntime PUBLIC ${BESS_RUNTIME_COMPONENT_TARGETS})

		
add_custom_command(TARGET BessRuntime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:dawn> 
				$<TARGET_FILE_DIR:BessRuntime>
)

foreach(component_target IN LISTS BESS_RUNTIME_COMPONENT_TARGETS)
    if(NOT component_target STREQUAL "BessExtDeps")
        target_compile_definitions(
            ${component_target} PRIVATE BessRuntime_EXPORTS
        )
        target_include_directories(
            ${component_target} PRIVATE
            "${BESS_RUNTIME_GENERATED_INCLUDE_DIR}"
        )
    endif()
endforeach()

set_target_properties(BessRuntime PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

set(Header_Files
)

source_group("include" FILES ${Header_Files})

set(Source_Files
	"main.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_executable(${PROJECT_NAME} ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES ENABLE_EXPORTS ON)
target_link_libraries(${PROJECT_NAME} PRIVATE BessRuntime)
add_dependencies(${PROJECT_NAME} BessRuntime besspybindings)

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ASSETS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")
		set(BESSPLUG_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bindings/bessplug")
		set(PLUGINS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "${ASSETS_DST_DIR}"
        COMMENT "Copying assets to build directory"
    )

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
					"${CMAKE_SOURCE_DIR}/plugins" "${PLUGINS_DST_DIR}"
        COMMENT "Copying plugins to build directory"
    )

		add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${CMAKE_SOURCE_DIR}/src/bessplug/py" "${BESSPLUG_DST_DIR}"
				COMMENT "Copying bindings to build directory"
		)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
	"${CMAKE_SOURCE_DIR}/src//include"
	"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
	"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
	"${CMAKE_SOURCE_DIR}/src/BessJson/include"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()
