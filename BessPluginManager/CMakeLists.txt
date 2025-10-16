set(PROJECT_NAME BessPluginManager)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set policy to suppress pybind11 warning
cmake_policy(SET CMP0148 NEW)

# Find Python before pybind11 to avoid warnings
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED)

set(Header_Files
    "include/plugin_manager.h"
    "include/plugin_interface.h"
    "include/python_bindings.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/plugin_manager.cpp"
    "src/plugin_interface.cpp"
    "src/python_bindings.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE pybind11::module Python3::Python)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    BESS_PLUGIN_MANAGER_EXPORTS
    $<$<CONFIG:Debug>:DEBUG>
)

if(UNIX)
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
        target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
    endif()
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
