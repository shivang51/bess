set(PROJECT_NAME BessSimEngine)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(Header_Files
    "include/digital_component.h"
    "include/types.h"
    "include/bess_uuid.h"
    "include/logger.h"
    "include/simulation_engine.h"
    "include/sim_engine_state.h"
    "include/simulation_engine_serializer.h"
    "include/component_catalog.h"
    "include/component_definition.h"
    "include/init_components.h" 
    "include/net/net.h" 
		"include/expression_evalutator/expr_evaluator.h"
		"include/commands/command.h"
		"include/commands/commands.h"
		"include/commands/commands_manager.h"
		"include/utils/string_utils.h"
		"include/command_processor/command_processor.h"
		"include/command_processor/commands_reg.h"
)
source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/simulation_engine.cpp"
		"src/sim_engine_state.cpp"
    "src/simulation_engine_serializer.cpp"
    "src/bess_uuid.cpp"
    "src/digital_component.cpp"
    "src/logger.cpp"
    "src/net/net.cpp" 
    "src/component_catalog.cpp"
    "src/component_definition.cpp"
		"src/commands/commands.cpp"
		"src/commands/commands_manager.cpp"
		"src/utils/string_utils.cpp"
		"src/command_processor/command_processor.cpp"
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

target_link_libraries(${PROJECT_NAME} PRIVATE BessPluginManager EventSystem BessJson)
add_dependencies(${PROJECT_NAME} BessPluginManager EventSystem BessJson)

target_compile_definitions(${PROJECT_NAME} PRIVATE BESS_EXPORTS)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include;"
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include;"
    "${CMAKE_SOURCE_DIR}/src/BessPluginManager/include;"
		"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
		"${CMAKE_SOURCE_DIR}/src/BessJson/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
	set_property(TARGET ${PROJECT_NAME} PROPERTY
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()
