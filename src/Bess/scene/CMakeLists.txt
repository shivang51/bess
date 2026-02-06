set(PROJECT_NAME BessScene)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"schematic_diagram.h"
	"scene.h"
	"viewport.h"
	"camera.h"
	"scene_serializer.h"
	"scene_state/scene_state.h"
	"scene_state/components/scene_component_types.h"
	"scene_state/components/scene_component.h"
	"scene_state/components/input_scene_component.h"
	"scene_state/components/slot_scene_component.h"
	"scene_state/components/sim_scene_component.h"
	"scene_state/components/non_sim_scene_component.h"
	"scene_state/components/connection_scene_component.h"
	"scene_state/components/conn_joint_scene_component.h"
	"scene_state/components/behaviours/drag_behaviour.h"
	"scene_state/components/behaviours/mouse_behaviour.h"
	"commands/set_input_command.h"
	"commands/del_connection_command.h"
	"commands/delete_comp_command.h"
	"commands/connect_command.h"
	"commands/add_command.h"
	"commands/create_group_command.h"
	"commands/delete_node_command.h"
	"commands/reparent_node_command.h"
	"renderer/vulkan/path_renderer.h"
	"renderer/vulkan/text_renderer.h"
	"renderer/vulkan/pipelines/quad_pipeline.h"
	"renderer/vulkan/pipelines/pipeline.h"
	"renderer/vulkan/pipelines/circle_pipeline.h"
	"renderer/vulkan/pipelines/grid_pipeline.h"
	"renderer/vulkan/pipelines/path_stroke_pipeline.h"
	"renderer/vulkan/pipelines/text_pipeline.h"
	"renderer/vulkan/pipelines/path_fill_pipeline.h"
	"renderer/material_renderer.h"
	"renderer/material.h"
	"renderer/msdf_font.h"
	"renderer/glyph_extractor.h"
	"renderer/font.h"
	"renderer/asset_loaders.h"
	"renderer/path.h"
)

source_group("include/scene" FILES ${Header_Files})

set(Source_Files
	"schematic_diagram.cpp"
	"scene_serializer.cpp"
	"viewport.cpp"
	"scene_state/scene_state.cpp"
	"scene_state/components/scene_component_types.cpp"
	"scene_state/components/scene_component.cpp"
	"scene_state/components/sim_scene_component.cpp"
	"scene_state/components/non_sim_scene_component.cpp"
	"scene_state/components/input_scene_component.cpp"
	"scene_state/components/slot_scene_component.cpp"
	"scene_state/components/connection_scene_component.cpp"
	"scene_state/components/conn_joint_scene_component.cpp"
	"scene.cpp"
	"scene_key_maps.cpp"
	"camera.cpp"
	"commands/connect_command.cpp"
	"commands/delete_comp_command.cpp"
	"commands/del_connection_command.cpp"
	"commands/set_input_command.cpp"
	"commands/add_command.cpp"
	"commands/create_group_command.cpp"
	"commands/delete_node_command.cpp"
	"commands/reparent_node_command.cpp"
	"renderer/vulkan/path_renderer.cpp"
	"renderer/vulkan/text_renderer.cpp"
	"renderer/vulkan/pipelines/path_stroke_pipeline.cpp"
	"renderer/vulkan/pipelines/text_pipeline.cpp"
	"renderer/vulkan/pipelines/quad_pipeline.cpp"
	"renderer/vulkan/pipelines/circle_pipeline.cpp"
	"renderer/vulkan/pipelines/path_fill_pipeline.cpp"
	"renderer/vulkan/pipelines/grid_pipeline.cpp"
	"renderer/vulkan/pipelines/pipeline.cpp"
	"renderer/font.cpp"
	"renderer/path.cpp"
	"renderer/glyph_extractor.cpp"
	"renderer/msdf_font.cpp"
	"renderer/material_renderer.cpp"
)

source_group("src/scene" FILES ${Source_Files})

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${EXT_OTHERS_SRC}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/src/Bess/"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
			"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
			"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
			"${CMAKE_SOURCE_DIR}/src/BessJson/include"
			"${CMAKE_SOURCE_DIR}/external/imgui"
			"${CMAKE_SOURCE_DIR}/external/implot"
			"${CMAKE_SOURCE_DIR}/external/freetype/include"
			"${CMAKE_SOURCE_DIR}/external/imgui/backends"
			"${CMAKE_SOURCE_DIR}/external/glm"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/stb_image/include"
			"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
)

target_link_libraries(${PROJECT_NAME} PUBLIC BessSimEngine BessVulkan BessPluginManager EventSystem BessJson)
add_dependencies(${PROJECT_NAME} BessSimEngine BessVulkan BessPluginManager EventSystem BessJson)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()


# NOT REQUIRED FOR NOW
# # Precompiled header for scene 
# set_source_files_properties(${Source_Files} PROPERTIES SKIP_PRECOMPILE_HEADERS OFF)
# set_source_files_properties(${Header_Files} PROPERTIES SKIP_PRECOMPILE_HEADERS OFF)
# set_source_files_properties(${Source_Files} PROPERTIES
# 		COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/scene_pch.h>
# )
# if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
#     target_precompile_headers(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_CURRENT_SOURCE_DIR}/scene_pch.h">)
# endif()
