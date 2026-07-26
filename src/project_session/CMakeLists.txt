set(PROJECT_NAME ProjectSession)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
    "include/project_session/components.h"
    "include/project_session/json_project_store.h"
    "include/project_session/project_document.h"
    "include/project_session/project_session.h"
    "include/project_session/project_store.h"
    "include/project_session/result.h"
    "include/project_session/scene_document.h"
    "include/project_session/session_event.h"
    "include/project_session/simulation_gateway.h"
    "include/project_session/transaction.h"
)

source_group("include/project_session" FILES ${Header_Files})

set(Source_Files
    "src/json_project_store.cpp"
    "src/project_document.cpp"
    "src/project_session.cpp"
    "src/scene_document.cpp"
    "src/simulation_gateway.cpp"
    "src/transaction.cpp"
)

source_group("src" FILES ${Source_Files})

add_library(${PROJECT_NAME} OBJECT
    ${Header_Files}
    ${Source_Files}
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE LOGGER_NAME="${PROJECT_NAME}"
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        common
        BessJson
        BessExtDeps
        BessSimEngine
        BessSimDriver
        EnTT::EnTT
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME}
        PRIVATE "/utf-8" "/Zc:__cplusplus"
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
