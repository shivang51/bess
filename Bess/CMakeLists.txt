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
    "../external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("External" FILES ${External})

set(Header_Files
"include/components_manager/component_type.h"
"include/components_manager/component_bank.h"
"include/components_manager/components_manager.h"
"include/components_manager/jcomponent_data.h"
"include/components/component.h"
"include/components/button.h"
"include/components/jcomponent.h"
"include/components/text_component.h"
"include/components/slot.h"
"include/components/output_probe.h"
"include/components/connection.h"
"include/components/connection_point.h"
"include/components/input_probe.h"
"include/components/clock.h"
"include/settings/themes.h"
"include/settings/settings.h"
"include/settings/viewport_theme.h"
"include/pages/page.h"
"include/pages/main_page/main_page.h"
"include/pages/main_page/main_page_state.h"
"include/pages/start_page/start_page.h"
"include/pages/page_identifier.h"
"include/application.h"
"include/simulator/simulator_engine.h"
"include/camera.h"
"include/common/helpers.h"
"include/common/bind_helpers.h"
"include/common/digital_state.h"
"include/project_file.h"
"include/ui/m_widgets.h"
"include/ui/icons/FontAwesomeIcons.h"
"include/ui/icons/MaterialIcons.h"
"include/ui/ui.h"
"include/ui/ui_main/properties_panel.h"
"include/ui/ui_main/settings_window.h"
"include/ui/ui_main/component_explorer.h"
"include/ui/ui_main/ui_main.h"
"include/ui/ui_main/popups.h"
"include/ui/ui_main/dialogs.h"
"include/events/application_event.h"
"include/window.h"
"include/application_state.h"
"include/renderer/renderer.h"
"include/renderer/font.h"
"include/renderer/gl/texture.h"
"include/renderer/gl/vertex.h"
"include/renderer/gl/shader.h"
"include/renderer/gl/framebuffer.h"
"include/renderer/gl/primitive_type.h"
"include/renderer/gl/vao.h"
"include/renderer/gl/gl_wrapper.h"
"include/renderer/gl/utils.h"

)
source_group("Shaders" FILES ${Shaders})

set(Source_Files
"src/components_manager/component_bank.cpp"
"src/components_manager/components_manager.cpp"
"src/components_manager/jcomponent_data.cpp"
"src/components/input_probe.cpp"
"src/components/button.cpp"
"src/components/component.cpp"
"src/components/output_probe.cpp"
"src/components/clock.cpp"
"src/components/text_component.cpp"
"src/components/slot.cpp"
"src/components/connection.cpp"
"src/components/connection_point.cpp"
"src/components/jcomponent.cpp"
"src/application_state.cpp"
"src/application.cpp"
"src/settings/viewport_theme.cpp"
"src/settings/themes.cpp"
"src/settings/settings.cpp"
"src/pages/page.cpp"
"src/pages/main_page/main_page.cpp"
"src/pages/main_page/main_page_state.cpp"
"src/pages/start_page/start_page.cpp"
"src/window.cpp"
"src/simulator/simulator_engine.cpp"
"src/common/helpers.cpp"
"src/ui/ui.cpp"
"src/ui/m_widgets.cpp"
"src/ui/ui_main/component_explorer.cpp"
"src/ui/ui_main/properties_panel.cpp"
"src/ui/ui_main/dialogs.cpp"
"src/ui/ui_main/ui_main.cpp"
"src/ui/ui_main/settings_window.cpp"
"src/ui/ui_main/popups.cpp"
"src/main.cpp"
"src/project_file.cpp"
"src/events/application_event.cpp"
"src/camera.cpp"
"src/renderer/font.cpp"
"src/renderer/gl/gl_wrapper.cpp"
"src/renderer/gl/shader.cpp"
"src/renderer/gl/texture.cpp"
"src/renderer/gl/framebuffer.cpp"
"src/renderer/gl/vao.cpp"
"src/renderer/renderer.cpp"
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
add_executable(${PROJECT_NAME} ${ALL_FILES}  "include/settings/themes.h" "include/components/clock.h" "src/components/clock.cpp")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})


use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE Bess)


set_target_properties(${PROJECT_NAME} PROPERTIES VS_GLOBAL_KEYWORD "Win32Proj")
if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE"
    )
elseif("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x86")
    set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE "TRUE")
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
        "${CMAKE_CURRENT_SOURCE_DIR}/../external/tinyfiledialogs;"
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
    if(UNIX)
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
        "freetype;"
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

add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION bin)
