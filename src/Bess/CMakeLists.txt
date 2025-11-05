set(PROJECT_NAME Bess)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_BUILD_RPATH "\$ORIGIN")
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

file(GLOB EXT_IMGUI_SRC
	"${CMAKE_SOURCE_DIR}/external/imgui/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implot/*.cpp"
)
source_group("external/imgui" FILES ${EXT_IMGUI_SRC})

set(EXT_OTHERS_SRC
	"${CMAKE_SOURCE_DIR}/external/stb_image/src/stb_image.cpp"
    "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
)

set(Header_Files
"assets.h"
"asset_manager/asset_id.h"
"asset_manager/asset_loader.h"
"asset_manager/asset_manager.h"
"settings/themes.h"
"settings/settings.h"
"settings/viewport_theme.h"
"pages/page.h"
"pages/main_page/main_page.h"
"pages/main_page/main_page_state.h"
"pages/start_page/start_page.h"
"pages/page_identifier.h"
"application.h"
"args_parser.h"
"crash_detail_helper.h"
"camera.h"
"common/helpers.h"
"common/bind_helpers.h"
"common/digital_state.h"
"project_file.h"
"ui/widgets/m_widgets.h"
"ui/icons/FontAwesomeIcons.h"
"ui/icons/ComponentIcons.h"
"ui/icons/MaterialIcons.h"
"ui/ui.h"
"ui/ui_main/properties_panel.h"
"ui/ui_main/project_settings_window.h"
"ui/ui_main/graph_view_window.h"
"ui/ui_main/scene_export_window.h"
"ui/ui_main/settings_window.h"
"ui/ui_main/component_explorer.h"
"ui/ui_main/project_explorer.h"
"ui/ui_main/ui_main.h"
"ui/ui_main/popups.h"
"ui/ui_main/dialogs.h"
"events/application_event.h"
"window.h"
"modules/schematic_gen/schematic_view.h"
"application_state.h"
"scene/scene.h"
"scene/schematic_diagram.h"
"scene/viewport.h"
"scene/scene_serializer.h"
"scene/artist/base_artist.h"
"scene/artist/schematic_artist.h"
"scene/artist/nodes_artist.h"
"scene/artist/artist_manager.h"
"scene/components/components.h"
"scene/components/json_converters.h"
"scene/components/non_sim_comp.h"
"scene/renderer/font.h"
"scene/renderer/material_renderer.h"
"scene/renderer/material.h"
"scene/renderer/path.h"
"scene/renderer/msdf_font.h"
"scene/renderer/glyph_extractor.h"
"scene/renderer/vulkan/pipelines/pipeline.h"
"scene/renderer/vulkan/pipelines/circle_pipeline.h"
"scene/renderer/vulkan/pipelines/grid_pipeline.h"
"scene/renderer/vulkan/pipelines/path_stroke_pipeline.h"
"scene/renderer/vulkan/pipelines/path_fill_pipeline.h"
"scene/renderer/vulkan/pipelines/quad_pipeline.h"
"scene/renderer/vulkan/pipelines/text_pipeline.h"
"scene/renderer/vulkan/path_renderer.h"
"scene/renderer/vulkan/text_renderer.h"
"scene/scene_pch.h"
"scene/commands/add_command.h"
"scene/commands/connect_command.h"
"scene/commands/delete_comp_command.h"
"scene/commands/del_connection_command.h"
"scene/commands/set_input_command.h"
"scene/commands/update_entt_comp_command.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
"asset_manager/asset_manager.cpp"
"application_state.cpp"
"application.cpp"
"settings/viewport_theme.cpp"
"settings/themes.cpp"
"settings/settings.cpp"
"pages/page.cpp"
"pages/main_page/main_page.cpp"
"pages/main_page/main_page_state.cpp"
"pages/start_page/start_page.cpp"
"window.cpp"
"modules/schematic_gen/schematic_view.cpp"
"common/helpers.cpp"
"ui/ui.cpp"
"ui/widgets/m_widgets.cpp"
"ui/ui_main/component_explorer.cpp"
"ui/ui_main/project_explorer.cpp"
"ui/ui_main/properties_panel.cpp"
"ui/ui_main/dialogs.cpp"
"ui/ui_main/ui_main.cpp"
"ui/ui_main/project_settings_window.cpp"
"ui/ui_main/graph_view_window.cpp"
"ui/ui_main/scene_export_window.cpp"
"ui/ui_main/settings_window.cpp"
"ui/ui_main/popups.cpp"
"main.cpp"
"project_file.cpp"
"events/application_event.cpp"
"camera.cpp"
"scene/components/non_sim_comp.cpp"
"scene/components/components.cpp"
"scene/scene.cpp"
"scene/schematic_diagram.cpp"
"scene/viewport.cpp"
"scene/scene_serializer.cpp"
"scene/artist/base_artist.cpp"
"scene/artist/schematic_artist.cpp"
"scene/artist/nodes_artist.cpp"
"scene/artist/artist_manager.cpp"
"scene/renderer/font.cpp"
"scene/renderer/material_renderer.cpp"
"scene/renderer/path.cpp"
"scene/renderer/msdf_font.cpp"
"scene/renderer/glyph_extractor.cpp"
"scene/renderer/vulkan/pipelines/pipeline.cpp"
"scene/renderer/vulkan/pipelines/circle_pipeline.cpp"
"scene/renderer/vulkan/pipelines/grid_pipeline.cpp"
"scene/renderer/vulkan/pipelines/path_fill_pipeline.cpp"
"scene/renderer/vulkan/pipelines/path_stroke_pipeline.cpp"
"scene/renderer/vulkan/pipelines/quad_pipeline.cpp"
"scene/renderer/vulkan/pipelines/text_pipeline.cpp"
"scene/renderer/vulkan/path_renderer.cpp"
"scene/renderer/vulkan/text_renderer.cpp"
"scene/commands/add_command.cpp"
"scene/commands/connect_command.cpp"
"scene/commands/delete_comp_command.cpp"
"scene/commands/del_connection_command.cpp"
"scene/commands/set_input_command.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Source_Files}
)

add_executable(${PROJECT_NAME} ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE BessSimEngine BessVulkan BessPluginManager)
add_dependencies(${PROJECT_NAME} BessSimEngine BessVulkan BessPluginManager)

file(GLOB_RECURSE SCENE_PCH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/*/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene/*/*/*.cpp"
)
set_source_files_properties(${Source_Files} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${External} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
set_source_files_properties(${SCENE_PCH_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS OFF)

set_source_files_properties(${SCENE_PCH_SOURCES} PROPERTIES
    COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/scene/scene_pch.h>
)

if("${BUILD_ARCH}" STREQUAL "x64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "gdi32;"
        "freetype;"
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/freetype/libs/windows"
    )

else()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
	    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
    else()
		target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
    endif()

    set(ADDITIONAL_LIBRARY_DEPENDENCIES
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
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
			"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
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
	)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(VULKAN REQUIRED vulkan)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC 
	${PNG_LIBRARIES}
	${GLFW_LIBRARIES}
	${X11_LIBRARIES}
	${VULKAN_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PUBLIC 
	${PNG_INCLUDE_DIRS}
	${VULKAN_INCLUDE_DIRS}
	${GLFW_INCLUDE_DIRS}
)


file(GLOB libtess2_src
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
)
add_library(libtess2_obj OBJECT ${libtess2_src})
target_include_directories(libtess2_obj PUBLIC 
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:libtess2_obj>)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

# Precompiled header for scene (C++ only)
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_CURRENT_SOURCE_DIR}/include/scene/scene_pch.h">)
endif()

