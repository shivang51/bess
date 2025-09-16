set(PROJECT_NAME Bess)

file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

file(GLOB EXT_IMGUI_SRC
    "../external/imgui/backends/*.cpp"
    "../external/imgui/*.cpp"
)
source_group("external/imgui" FILES ${EXT_IMGUI_SRC})

set(EXT_OTHERS_SRC
    "../external/glad/src/glad.c"
    "../external/stb_image/src/stb_image.cpp"
    "../external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
)

set(Header_Files
"include/assets.h"
"include/asset_manager/asset_id.h"
"include/asset_manager/asset_loader.h"
"include/asset_manager/asset_manager.h"
"include/settings/themes.h"
"include/settings/settings.h"
"include/settings/viewport_theme.h"
"include/pages/page.h"
"include/pages/main_page/main_page.h"
"include/pages/main_page/main_page_state.h"
"include/pages/start_page/start_page.h"
"include/pages/page_identifier.h"
"include/application.h"
"include/camera.h"
"include/common/helpers.h"
"include/common/bind_helpers.h"
"include/common/digital_state.h"
"include/project_file.h"
"include/ui/m_widgets.h"
"include/ui/icons/FontAwesomeIcons.h"
"include/ui/icons/ComponentIcons.h"
"include/ui/icons/MaterialIcons.h"
"include/ui/ui.h"
"include/ui/ui_main/properties_panel.h"
"include/ui/ui_main/project_settings_window.h"
"include/ui/ui_main/settings_window.h"
"include/ui/ui_main/component_explorer.h"
"include/ui/ui_main/project_explorer.h"
"include/ui/ui_main/ui_main.h"
"include/ui/ui_main/popups.h"
"include/ui/ui_main/dialogs.h"
"include/events/application_event.h"
"include/window.h"
"include/modules/schematic_gen/schematic_view.h"
"include/application_state.h"
"include/scene/renderer/renderer.h"
"include/scene/scene.h"
"include/scene/scene_serializer.h"
"include/scene/artist.h"
"include/scene/components/components.h"
"include/scene/components/json_converters.h"
"include/scene/components/non_sim_comp.h"
"include/scene/renderer/font.h"
"include/scene/renderer/msdf_font.h"
"include/scene/renderer/gl/texture.h"
"include/scene/renderer/gl/subtexture.h"
"include/scene/renderer/gl/vertex.h"
"include/scene/renderer/gl/shader.h"
"include/scene/renderer/gl/fb_attachment.h"
"include/scene/renderer/gl/framebuffer.h"
"include/scene/renderer/gl/primitive_type.h"
"include/scene/renderer/gl/vao.h"
"include/scene/renderer/gl/gl_wrapper.h"
"include/scene/renderer/gl/utils.h"
"include/scene/renderer/gl/instanced_vao.h"
"include/scene/renderer/gl/batch_vao.h"
"include/scene/renderer/gl/vaos.h"
"include/scene/commands/add_command.h"
"include/scene/commands/connect_command.h"
"include/scene/commands/delete_comp_command.h"
"include/scene/commands/del_connection_command.h"
"include/scene/commands/set_input_command.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
"src/asset_manager/asset_manager.cpp"
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
"src/modules/schematic_gen/schematic_view.cpp"
"src/common/helpers.cpp"
"src/ui/ui.cpp"
"src/ui/m_widgets.cpp"
"src/ui/ui_main/component_explorer.cpp"
"src/ui/ui_main/project_explorer.cpp"
"src/ui/ui_main/properties_panel.cpp"
"src/ui/ui_main/dialogs.cpp"
"src/ui/ui_main/ui_main.cpp"
"src/ui/ui_main/project_settings_window.cpp"
"src/ui/ui_main/settings_window.cpp"
"src/ui/ui_main/popups.cpp"
"src/main.cpp"
"src/project_file.cpp"
"src/events/application_event.cpp"
"src/camera.cpp"
"src/scene/components/non_sim_comp.cpp"
"src/scene/scene.cpp"
"src/scene/scene_serializer.cpp"
"src/scene/artist.cpp"
"src/scene/renderer/font.cpp"
"src/scene/renderer/msdf_font.cpp"
"src/scene/renderer/gl/gl_wrapper.cpp"
"src/scene/renderer/gl/shader.cpp"
"src/scene/renderer/gl/texture.cpp"
"src/scene/renderer/gl/subtexture.cpp"
"src/scene/renderer/gl/fb_attachment.cpp"
"src/scene/renderer/gl/framebuffer.cpp"
"src/scene/renderer/gl/vao.cpp"
"src/scene/renderer/renderer.cpp"
"src/scene/renderer/gl/vaos.cpp"
"src/scene/commands/add_command.cpp"
"src/scene/commands/connect_command.cpp"
"src/scene/commands/delete_comp_command.cpp"
"src/scene/commands/del_connection_command.cpp"
"src/scene/commands/set_input_command.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Source_Files}
)

add_executable(${PROJECT_NAME} ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE BessSimEngine)
add_dependencies(${PROJECT_NAME} BessSimEngine)
if("${BUILD_ARCH}" STREQUAL "x64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "glfw3;"
        "opengl32;"
        "gdi32;"
        "freetype;"
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/glfw/lib-vc2022"
        "${CMAKE_SOURCE_DIR}/external/freetype/libs/windows"
    )

else()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
	    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
    else()
		target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
    endif()

    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "glfw;"
        "GL;"
        "freetype;"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ASSETS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")

    file(MAKE_DIRECTORY "${ASSETS_DST_DIR}")

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "${ASSETS_DST_DIR}"
        COMMENT "Copying assets to build directory"
    )
endif()

if("${BUILD_ARCH}" STREQUAL "x64")
	target_include_directories(${PROJECT_NAME} PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}/include"
			"${CMAKE_SOURCE_DIR}/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/external/imgui"
			"${CMAKE_SOURCE_DIR}/external/freetype/include"
			"${CMAKE_SOURCE_DIR}/external/imgui/backends"
			"${CMAKE_SOURCE_DIR}/external/glm"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/glfw/include"
			"${CMAKE_SOURCE_DIR}/external/glad/include"
			"${CMAKE_SOURCE_DIR}/external/stb_image/include"
			"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
			"${CMAKE_SOURCE_DIR}/external/entt/include"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include;"
	)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

