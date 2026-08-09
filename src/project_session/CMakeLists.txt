cmake_minimum_required(VERSION 3.27 FATAL_ERROR)

project(project_session LANGUAGES CXX)

add_library(project_session OBJECT
    include/project_session/edit_hooks.h
    include/project_session/project_document.h
    include/project_session/project_session.h
    include/project_session/project_transaction.h
    include/project_session/status.h
    src/edit_hooks.cpp
    src/project_document.cpp
    src/project_session.cpp
    src/project_transaction.cpp
    src/session_priv.h
)

add_library(project_session::project_session ALIAS project_session)

target_compile_features(project_session PUBLIC cxx_std_23)
set_target_properties(project_session PROPERTIES
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(project_session
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_SOURCE_DIR}/src/event_system/include"
        "${CMAKE_SOURCE_DIR}/src/sim_driver/include"
        "${CMAKE_SOURCE_DIR}/src/sim_engine/include"
        "${CMAKE_SOURCE_DIR}/external/imgui"
        "${CMAKE_SOURCE_DIR}/external/freetype/include"
)

target_link_libraries(project_session PUBLIC
    common
    BessJson
    BessSimEngine
    BessExtDeps
)
target_link_libraries(project_session PRIVATE BessCore::Api)
target_link_libraries(project_session INTERFACE BessCore)

target_compile_definitions(project_session PRIVATE
    LOGGER_NAME="project_session"
)

if(MSVC)
    target_compile_options(project_session PRIVATE /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(project_session PRIVATE
        -Wall
        -Wextra
    )
endif()
