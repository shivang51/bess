set(PROJECT_NAME BessCore)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"include/bess_core/g_app_context.h"
	"include/bess_core/viewport.h"
	"include/bess_core/connection_service.h"

	"include/bess_core/style/bess_theme.h"
	"include/bess_core/style/color_scheme.h"
	"include/bess_core/style/color_utils.h"

	"include/bess_core/scene/schematic_diagram.h"
	"include/bess_core/scene/scene.h"
	"include/bess_core/scene/scene_draw_context.h"
	"include/bess_core/scene/widgets/scene_widgets_internal.h"
	"include/bess_core/scene/widgets/scene_widgets.h"
	"include/bess_core/scene/scene_types.h"
	"include/bess_core/scene/scene_events.h"
	"include/bess_core/scene/camera.h"
	"include/bess_core/scene/scene_event_builder.h"
	"include/bess_core/scene/scene_layer.h"
	"include/bess_core/scene/scene_event.h"
	"include/bess_core/scene/scene_ser_reg.h"
	"include/bess_core/scene/scene_state/components/scene_component.h"
	"include/bess_core/scene/scene_state/components/behaviours/mouse_behaviour.h"
	"include/bess_core/scene/scene_state/components/behaviours/drag_behaviour.h"
	"include/bess_core/scene/scene_state/components/scene_component_types.h"
	"include/bess_core/scene/scene_state/components/styles/comp_style.h"
	"include/bess_core/scene/scene_state/components/styles/sim_comp_style.h"
	"include/bess_core/scene/scene_state/scene_state.h"
	"include/bess_core/scene/scene_serializer.h"
	"include/bess_core/scene/scene_draw_helpers.h"
	"include/bess_core/scene/layers/components_layer.h"
	"include/bess_core/scene/layers/scratch_layer.h"
	"include/bess_core/scene/layers/screen_space_overlay_layer.h"
	"include/bess_core/scene/layers/overlay_layer.h"
	"include/bess_core/scene/layers/hover_layer.h"
	"include/bess_core/scene/layers/grid_layer.h"
	"include/bess_core/scene/layers/scene_widgets_layer.h"
	"include/bess_core/scene/layers/interaction_layer.h"

	"include/bess_core/scene_driver.h"
	"include/bess_core/project_context.h"
	"include/bess_core/copy_paste_service.h"

	"include/bess_core/asset_manager/asset_loader.h"
	"include/bess_core/asset_manager/asset_manager.h"
	"include/bess_core/asset_manager/asset_id.h"

	"include/bess_core/renderer/texture.h"
	"include/bess_core/renderer/colors.h"
	"include/bess_core/renderer/renderer_types.h"
	"include/bess_core/renderer/renderer_path.h"
	"include/bess_core/renderer/subtexture.h"
	"include/bess_core/renderer/shader.h"
	"include/bess_core/renderer/renderer_2d.h"
	"include/bess_core/renderer/msdf_font.h"
	"include/bess_core/renderer/glyph_extractor.h"
	"include/bess_core/renderer/font.h"
	
	"include/bess_core/settings/settings.h"
	"include/bess_core/settings/themes.h"
	"include/bess_core/settings/viewport_theme.h"
	
	"include/bess_core/sub_systems/input_sub_system.h"
	"include/bess_core/sub_systems/input_sub_system_types.h"
)

source_group("include/bess_core" FILES ${Header_Files})

set(Source_Files
	"src/bess_core/asset_manager.cpp"
	"src/bess_core/project_context.cpp"
	"src/bess_core/connection_service.cpp"
	"src/bess_core/copy_paste_service.cpp"
	
	"src/bess_core/style/bess_theme.cpp"
	"src/bess_core/style/color_scheme.cpp"
	"src/bess_core/style/color_utils.cpp"
	
	"src/bess_core/scene/schematic_diagram.cpp"
	"src/bess_core/scene/scene_serializer.cpp"
	"src/bess_core/scene/widgets/scene_widgets_text_box.cpp"
	"src/bess_core/scene/widgets/scene_widgets_controls.cpp"
	"src/bess_core/scene/widgets/scene_widgets_slider.cpp"
	"src/bess_core/scene/widgets/scene_widgets_input.cpp"
	"src/bess_core/scene/widgets/scene_widgets_state.cpp"
	"src/bess_core/scene/widgets/scene_widgets_dropdown.cpp"
	"src/bess_core/scene/scene_ser_reg.cpp"
	"src/bess_core/scene/scene_state/components/scene_component_types.cpp"
	"src/bess_core/scene/scene_state/components/scene_component.cpp"
	"src/bess_core/scene/scene_state/scene_state.cpp"
	"src/bess_core/scene/scene.cpp"
	"src/bess_core/scene/camera.cpp"
	"src/bess_core/scene/scene_event_builder.cpp"
	"src/bess_core/scene/layers/components_layer.cpp"
	"src/bess_core/scene/layers/scratch_layer.cpp"
	"src/bess_core/scene/layers/hover_layer.cpp"
	"src/bess_core/scene/layers/scene_widgets_layer.cpp"
	"src/bess_core/scene/layers/overlay_layer.cpp"
	"src/bess_core/scene/layers/interaction_layer.cpp"
	"src/bess_core/scene/layers/grid_layer.cpp"
	"src/bess_core/scene/layers/screen_space_overlay_layer.cpp"
	
	"src/bess_core/scene_driver.cpp"
	"src/bess_core/g_app_context.cpp"
	
	"src/bess_core/renderer/font.cpp"
	"src/bess_core/renderer/renderer_path.cpp"
	"src/bess_core/renderer/shader.cpp"
	"src/bess_core/renderer/glyph_extractor.cpp"
	"src/bess_core/renderer/texture.cpp"
	"src/bess_core/renderer/renderer.cpp"
	"src/bess_core/renderer/renderer_types.cpp"
	
	"src/bess_core/settings/settings.cpp"
	"src/bess_core/settings/themes.cpp"
	"src/bess_core/settings/viewport_theme.cpp"
	
	"src/bess_core/sub_systems/input_sub_system.cpp"
	
	"src/bess_core/scene/scene_ui/layout.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
	${Header_Files}
	${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE BESS_EXPORTS)
target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PUBLIC common BessJson BessExtDeps EventSystem)

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_SOURCE_DIR}/src/bess/application"
	"${CMAKE_SOURCE_DIR}/src/bess/ui"
	"${CMAKE_SOURCE_DIR}/src/bess"
	"${CMAKE_SOURCE_DIR}/src/bess/scene"
	"${CMAKE_SOURCE_DIR}/src/event_system/include"
	"${CMAKE_SOURCE_DIR}/src/command_system/include"
	"${CMAKE_SOURCE_DIR}/external/freetype/include"
	"${CMAKE_SOURCE_DIR}/external/imgui"
	"${CMAKE_SOURCE_DIR}/src/sim_driver/include"
	"${CMAKE_SOURCE_DIR}/src/digital_sim_driver/include"
	"${CMAKE_SOURCE_DIR}/src/sim_engine/include"
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
