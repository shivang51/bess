set(PROJECT_NAME BessApp)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"args_parser.h"
	
	"pages/page.h"
	"pages/main_page/main_page.h"
	"pages/main_page/main_page_state.h"
	"pages/main_page/main_page_edit_hooks.h"
	"pages/main_page/comp_edit.h"
	"pages/main_page/project_model.h"
	"pages/main_page/module_edit.h"
	"pages/main_page/scene_components/conn_joint_scene_component.h"
	"pages/main_page/scene_components/group_scene_component.h"
	"pages/main_page/scene_components/slot_scene_component.h"
	"pages/main_page/scene_components/slot_probe_scene_component.h"
	"pages/main_page/scene_components/proxy_slot_component.h"
	"pages/main_page/scene_components/non_sim_scene_component.h"
	"pages/main_page/scene_components/text_scene_component.h"
	"pages/main_page/scene_components/image_scene_component.h"
	"pages/main_page/scene_components/monitor_scene_comp.h"
	"pages/main_page/scene_components/scene_component_draw_resources.h"
	"pages/main_page/scene_components/input_scene_component.h"
	"pages/main_page/scene_components/connection_scene_component.h"
	"pages/main_page/scene_components/sim_scene_component.h"
	"pages/main_page/scene_components/module_scene_component.h"
	"pages/main_page/scene_components/scene_comp_types.h"
	"pages/main_page/services/connection_service.h"
	"pages/main_page/services/copy_paste_service.h"
	"pages/main_page/services/hierarchical_scene_layout.h"
	"pages/main_page/verilog_scene_import.h"
	"pages/start_page/start_page.h"
	
	"crash_detail_helper.h"
	"application.h"
	"assets.h"
	"services/window_drop_service/window_drop_service.h"
	"services/plugin_service/plugin_service.h"
	
	"ui/ui_hook.h"
	"ui/icons/ComponentIcons.h"
	"ui/icons/FontAwesomeIcons_Remapped.h"
	"ui/icons/CodIcons.h"
	"ui/icons/ComponentIcons_Remapped.h"
	"ui/icons/CodIcons_Remapped.h"
	"ui/icons/MaterialIcons_Remapped.h"
	"ui/icons/FontAwesomeIcons.h"
	"ui/icons/MaterialIcons.h"
	"ui/widgets/m_widgets.h"
	"ui/dock_ids.h"
	"ui/ui_panel.h"
	"ui/ui.h"
	"ui/project_api.h"
	"ui/ui_main/properties_panel.h"
	"ui/ui_main/stamp_data_viewer.h"
	"ui/ui_main/graph_view_window.h"
	"ui/ui_main/project_settings_window.h"
	"ui/ui_main/log_window.h"
	"ui/ui_main/debug_panel.h"
	"ui/ui_main/scene_viewport_panel.h"
	"ui/ui_main/component_explorer.h"
	"ui/ui_main/scene_export_window.h"
	"ui/ui_main/ui_main.h"
	"ui/ui_main/settings_window/settings_window.h"
	"ui/ui_main/popups.h"
	"ui/ui_main/dialogs.h"
	"ui/ui_main/project_explorer.h"
	
	"events/application_event.h"
	"window.h"
	"sub_systems/renderer_context.h"
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	list(APPEND Header_Files
		"platform/x11/x11_window_drop_handler.h"
	)
endif()

source_group("include" FILES ${Header_Files})

set(Source_Files
	"application.cpp"
	
	"pages/page.cpp"
	"pages/main_page/verilog_scene_import.cpp"
	"pages/main_page/main_page_state.cpp"
	"pages/main_page/main_page.cpp"
	"pages/main_page/main_page_edit_hooks.cpp"
	"pages/main_page/project_model.cpp"
	"pages/main_page/module_edit.cpp"
	"pages/main_page/scene_components/slot_scene_component.cpp"
	"pages/main_page/scene_components/sim_scene_component.cpp"
	"pages/main_page/scene_components/monitor_scene_comp.cpp"
	"pages/main_page/scene_components/proxy_slot_component.cpp"
	"pages/main_page/scene_components/conn_joint_scene_component.cpp"
	"pages/main_page/scene_components/group_scene_component.cpp"
	"pages/main_page/scene_components/connection_scene_component.cpp"
	"pages/main_page/scene_components/non_sim_scene_component.cpp"
	"pages/main_page/scene_components/text_scene_component.cpp"
	"pages/main_page/scene_components/image_scene_component.cpp"
	"pages/main_page/scene_components/module_scene_component.cpp"
	"pages/main_page/scene_components/input_scene_component.cpp"
	"pages/main_page/scene_components/slot_probe_scene_component.cpp"
	"pages/main_page/services/connection_service.cpp"
	"pages/main_page/services/copy_paste_service.cpp"
	"pages/main_page/services/hierarchical_scene_layout.cpp"
	
	"window.cpp"
	
	"services/window_drop_service/window_drop_service.cpp"
	"services/plugin_service/plugin_service.cpp"
	
	"ui/ui.cpp"
	"ui/project_api.cpp"
	"ui/widgets/m_widgets.cpp"
	"ui/ui_panel.cpp"
	"ui/ui_hook.cpp"
	"ui/ui_main/component_explorer.cpp"
	"ui/ui_main/properties_panel.cpp"
	"ui/ui_main/stamp_data_viewer.cpp"
	"ui/ui_main/project_explorer.cpp"
	"ui/ui_main/log_window.cpp"
	"ui/ui_main/scene_viewport_panel.cpp"
	"ui/ui_main/project_settings_window.cpp"
	"ui/ui_main/dialogs.cpp"
	"ui/ui_main/ui_main.cpp"
	"ui/ui_main/scene_export_window.cpp"
	"ui/ui_main/scene_viewport_panel.scene.cpp"
	"ui/ui_main/debug_panel.cpp"
	"ui/ui_main/settings_window/settings_window.cpp"
	"ui/ui_main/popups.cpp"
	"ui/ui_main/graph_view_window.cpp"
	
	"events/application_event.cpp"
	"sub_systems/renderer_context.cpp"
)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	list(APPEND Source_Files
		"platform/x11/x11_window_drop_handler.cpp"
	)
endif()

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC common BessCore BessExtDeps BessJson BessSimEngine  BessPluginManager EventSystem BessSimDriver DigitalSimDriver MathSimDriver bverilog BessWgpu)
# BessApp is an object library, so normal PRIVATE link dependencies are
# forwarded to targets that consume its objects. project_session is already
# linked into BessRuntime; forwarding its static archive to runtime consumers
# makes MSVC link the same exported definitions into the executable again.
target_link_libraries(${PROJECT_NAME} PRIVATE
	$<COMPILE_ONLY:project_session>
)

if(WIN32)
	target_link_libraries(${PROJECT_NAME} PUBLIC
		advapi32
		shell32
		uuid
	)
endif()

add_dependencies(${PROJECT_NAME} common BessCore BessExtDeps BessJson BessSimEngine  BessPluginManager BessSimDriver DigitalSimDriver MathSimDriver bverilog BessWgpu)

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()
