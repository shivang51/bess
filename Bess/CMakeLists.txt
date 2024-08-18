set(PROJECT_NAME Bess)
set(CMAKE_CXX_STANDARD 20)
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
    "../external/imgui/imgui_demo.cpp"
    "../external/stb_image/src/stb_image.cpp"
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
    "include/components/output_probe.h"
    "include/components/slot.h"
    "include/components_manager/component_type.h"
    "include/components_manager/components_manager.h"
    "include/renderer/gl/framebuffer.h"
    "include/renderer/gl/gl_wrapper.h"
    "include/renderer/gl/shader.h"
    "include/renderer/gl/utils.h"
    "include/renderer/gl/vao.h"
    "include/renderer/gl/vertex.h"
    "include/renderer/gl/texture.h"
    "include/renderer/renderer.h"
    "include/renderer/font.h"
    "include/renderer/gl/primitive_type.h"
    "include/ui/ui.h"
    "include/ui/icons/FontAwesomeIcons.h"
    "include/window.h"
    "include/common/bind_helpers.h"
    "include/common/helpers.h"
    "include/common/digital_state.h"
    "include/components/jcomponent.h"
    "include/components_manager/jcomponent_data.h"
    "include/components_manager/component_bank.h"
    "include/simulator/simulator_engine.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Shaders
    "../assets/shaders/circle_frag.glsl"
    "../assets/shaders/curve_frag.glsl"
    "../assets/shaders/quad_frag.glsl"
    "../assets/shaders/quad_vert.glsl"
    "../assets/shaders/vert.glsl"
    "../assets/shaders/font_frag.glsl"
    "../assets/shaders/grid_frag.glsl"
    "../assets/shaders/grid_vert.glsl"
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
    "src/components/slot.cpp"
    "src/components_manager/components_manager.cpp"
    "src/renderer/gl/framebuffer.cpp"
    "src/renderer/gl/gl_wrapper.cpp"
    "src/renderer/gl/shader.cpp"
    "src/renderer/gl/vao.cpp"
    "src/renderer/gl/texture.cpp"
    "src/main.cpp"
    "src/renderer/renderer.cpp"
    "src/ui/ui.cpp"
    "src/window.cpp"
    "src/components/output_probe.cpp"
    "src/renderer/font.cpp"
    "src/common/helpers.cpp"
    "src/components_manager/jcomponent_data.cpp"
    "src/components_manager/component_bank.cpp"
    "src/components/jcomponent.cpp"
    "src/simulator/simulator_engine.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Shaders}
    ${Source_Files}
 "include/ui/component_explorer.h" "src/ui/component_explorer.cpp" "include/project_file.h" "src/project_file.cpp" "include/ui/dialogs.h" "src/ui/dialogs.cpp" "include/ui/icons/MaterialIcons.h" "src/ui/popups.cpp" "include/ui/popups.h" "include/components/button.h" "src/components/button.cpp" "include/ui/properties_panel.h" "src/ui/properties_panel.cpp")

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES} )
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})


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
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/uuid;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/freetype/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/imgui/backends;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glm;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/json;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glfw/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include;"
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/stb_image/include;"
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
    set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
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
            COMMAND $<CONFIG:Release> xcopy $<SHELL_PATH:${CMAKE_SOURCE_DIR}/>assets ${OUTPUT_DIRECTORY}\\assets\\  /E/Y
        )
    else()
        add_custom_command_if(
            TARGET ${PROJECT_NAME}
            POST_BUILD
            COMMANDS
            COMMAND $<CONFIG:Release> cp -r $<SHELL_PATH:${CMAKE_SOURCE_DIR}/>assets ${OUTPUT_DIRECTORY}/assets
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
        "freetype;"
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
        "${CMAKE_SOURCE_DIR}/external/freetype/libs/windows"
    )
endif()

