set(PROJECT_NAME BessSimEngine)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(Header_Files
    "include/digital_component.h"
    "include/types.h"
    "include/simulation_engine.h"
		"include/driver_registry.h"
    "include/component_catalog.h"
    "include/component_definition.h"
		"include/expression_evalutator/expr_evaluator.h"
		"include/utils/string_utils.h"
)
source_group("include" FILES ${Header_Files})

set(Source_Files
		"src/driver_registry.cpp"
	  "src/expr_evaluator.cpp"
    "src/simulation_engine.cpp"
    "src/digital_component.cpp"
    "src/component_catalog.cpp"
    "src/component_definition.cpp"
		"src/utils/string_utils.cpp"
)
source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Shaders}
    ${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES} )
if(UNIX)
if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE BessPluginManager EventSystem BessJson common BessSimDriver)
add_dependencies(${PROJECT_NAME} BessPluginManager EventSystem BessJson common BessSimDriver)

target_compile_definitions(${PROJECT_NAME} PRIVATE BESS_EXPORTS)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include;"
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include;"
    "${CMAKE_SOURCE_DIR}/src/bverilog/include;"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
	set_property(TARGET ${PROJECT_NAME} PROPERTY
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()
