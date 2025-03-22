set(PROJECT_NAME BessSimEngine)
set(CMAKE_CXX_STANDARD 23)


set(External
)
source_group("External" FILES ${External})

set(Header_Files
    "include/entt_components.h"
    "include/types.h"
    "include/bess_uuid.h"
    "include/properties.h"
    "include/simulation_engine.h"
    "include/component_catalog.h"
    "include/component_definition.h"
    "include/component_types.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "src/simulation_engine.cpp"
    "src/bess_uuid.cpp"
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

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/json;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/entt/include;"
)
