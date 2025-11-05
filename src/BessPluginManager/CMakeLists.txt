set(PROJECT_NAME BessPluginManager)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set policy to suppress pybind11 warning
cmake_policy(SET CMP0148 NEW)

find_package(Python3 COMPONENTS Interpreter Development Development.Module Development.Embed REQUIRED)
set(PYBIND11_FINDPYTHON ON)
find_package(pybind11 REQUIRED)

set(Header_Files
    "include/plugin_manager.h"
    "include/plugin_handle.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/plugin_manager.cpp"
    "src/plugin_handle.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC 
	Python3::Python
	pybind11::embed
)


target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
			"${CMAKE_SOURCE_DIR}/external/imgui"
			"${CMAKE_SOURCE_DIR}/external/implot"
			"${CMAKE_SOURCE_DIR}/external/freetype/include"
			"${CMAKE_SOURCE_DIR}/external/imgui/backends"
			"${CMAKE_SOURCE_DIR}/external/glm"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/stb_image/include"
			"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
			"${CMAKE_SOURCE_DIR}/external/entt/include"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/Bess/"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
)

target_include_directories(BessPluginManager
    PUBLIC include
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
