set(PROJECT_NAME BessSimEngine)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(External
)
source_group("External" FILES ${External})

set(Header_Files
    "include/entt_components.h"
    "include/types.h"
    "include/bess_uuid.h"
    "include/logger.h"
    "include/properties.h"
    "include/entt_registry_serializer.h"
    "include/simulation_engine.h"
    "include/simulation_engine_serializer.h"
    "include/component_catalog.h"
    "include/component_definition.h"
    "include/component_types.h"
    "include/init_components.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "src/simulation_engine.cpp"
    "src/simulation_engine_serializer.cpp"
    "src/bess_uuid.cpp"
    "src/logger.cpp"
		"src/entt_registry_serializer.cpp"
    "src/component_catalog.cpp"
    "src/component_definition.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Shaders}
    ${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})
if(UNIX)
if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
endif()
endif()
target_compile_definitions(${PROJECT_NAME} PRIVATE BESS_EXPORTS)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/json;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/entt/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/spdlog/include;"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
	set_property(TARGET ${PROJECT_NAME} PROPERTY
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
	)
endif()
