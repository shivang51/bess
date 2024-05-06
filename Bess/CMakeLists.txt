set(PROJECT_NAME Bess)

################################################################################
# Source groups
################################################################################
set(External
    "../external/glad/src/glad.c"
    "../external/imgui/backends/imgui_impl_glfw.cpp"
    "../external/imgui/backends/imgui_impl_opengl3.cpp"
    "../external/imgui/imgui.cpp"
    "../external/imgui/imgui_draw.cpp"
    "../external/imgui/imgui_tables.cpp"
    "../external/imgui/imgui_widgets.cpp"
)
source_group("External" FILES ${External})

set(Header_Files
    "include/application.h"
    "include/application_state.h"
    "include/camera.h"
    "include/common/theme.h"
    "include/components/component.h"
    "include/components/connection.h"
    "include/components/input_probe.h"
    "include/components/nand_gate.h"
    "include/components/slot.h"
    "include/components_manager/component_type.h"
    "include/components_manager/components_manager.h"
    "include/gl/framebuffer.h"
    "include/gl/gl_wrapper.h"
    "include/gl/shader.h"
    "include/gl/utils.h"
    "include/gl/vao.h"
    "include/gl/vertex.h"
    "include/renderer/renderer.h"
    "include/ui.h"
    "include/window.h"
        include/components/output_probe.h
)
source_group("Header Files" FILES ${Header_Files})

set(Shaders
    "../assets/shaders/circle_frag.glsl"
    "../assets/shaders/curve_frag.glsl"
    "../assets/shaders/quad_frag.glsl"
    "../assets/shaders/quad_frag1.glsl"
    "../assets/shaders/vert.glsl"
)
source_group("Shaders" FILES ${Shaders})

set(Source_Files
    "src/application.cpp"
    "src/application_state.cpp"
    "src/camera.cpp"
    "src/common/theme.cpp"
    "src/components/component.cpp"
    "src/components/connection.cpp"
    "src/components/input_probe.cpp"
    "src/components/nand_gate.cpp"
    "src/components/slot.cpp"
    "src/components_manager/components_manager.cpp"
    "src/gl/framebuffer.cpp"
    "src/gl/gl_wrapper.cpp"
    "src/gl/shader.cpp"
    "src/gl/vao.cpp"
    "src/main.cpp"
    "src/renderer/renderer.cpp"
    "src/ui.cpp"
    "src/window.cpp"
        src/components/output_probe.cpp
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Shaders}
    ${Source_Files}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})


use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE Bess)


set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
endif()
################################################################################
# Include directories
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/uuid_v4;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glm;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glfw/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
endif()

################################################################################
# Compile definitions
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:Debug>:"
            "_DEBUG"
        ">"
        "$<$<CONFIG:Release>:"
            "NDEBUG"
        ">"
        "WIN32;"
        "_CONSOLE;"
        "UNICODE;"
        "_UNICODE"
    )
endif()

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_compile_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /Oi;
                /Gy
            >
            /permissive-;
            /sdl;
            /W3;
            ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
            ${DEFAULT_CXX_EXCEPTION_HANDLING}
        )
    endif()
    if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
        target_link_options(${PROJECT_NAME} PRIVATE
            $<$<CONFIG:Release>:
                /OPT:REF;
                /OPT:ICF
            >
            /DEBUG;
            /SUBSYSTEM:CONSOLE
        )
    endif()
endif()

################################################################################
# Post build events
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    if(NOT UNIX)
        add_custom_command_if(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMANDS
            COMMAND $<CONFIG:Release> xcopy $<SHELL_PATH:${CMAKE_SOURCE_DIR}/>assets ${OUTPUT_DIRECTORY}assets\\  /E/Y
        )
    else()
        add_custom_command_if(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMANDS
            COMMAND $<CONFIG:Release> cp -r $<SHELL_PATH:${CMAKE_SOURCE_DIR}/>assets ${OUTPUT_DIRECTORY}assets
        )
    endif()
endif()

################################################################################
# Dependencies
################################################################################
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64" AND NOT UNIX)
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "glfw3;"
        "opengl32;"
        "gdi32;"
    )
else()
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "glfw;"
        "GL;"
    )
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

SET(CMAKE_CXX_FLAGS  "-march=native")

if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022"
    )
endif()

