set(PROJECT_NAME BessScene)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(EXT_OTHERS_SRC
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
		${EXT_OTHERS_SRC}
)

set(Header_Files
)

source_group("include" FILES ${Header_Files})

set(Source_Files
"schematic_diagram.cpp"
"components/non_sim_comp.cpp"
"components/components.cpp"
"scene_serializer.cpp"
"viewport.cpp"
"artist/schematic_artist.cpp"
"artist/base_artist.cpp"
"artist/nodes_artist.cpp"
"artist/artist_manager.cpp"
"scene_state/scene_state.cpp"
"scene_state/components/scene_component_types.cpp"
"scene_state/components/scene_component.cpp"
"scene_state/components/sim_scene_component.cpp"
"scene_state/components/input_scene_component.cpp"
"scene_state/components/slot_scene_component.cpp"
"scene_state/components/connection_scene_component.cpp"
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

source_group("src" FILES ${Source_Files})

file(GLOB_RECURSE BESS_SCENE_SOURCES
		"${CMAKE_SOURCE_DIR}/src/Bess/scene/**/*.cpp"
)

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${EXT_OTHERS_SRC}
		${BESS_SCENE_SOURCES}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/src/Bess/"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
			"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
			"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
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

target_link_libraries(${PROJECT_NAME} PUBLIC BessSimEngine BessVulkan BessPluginManager EventSystem)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

