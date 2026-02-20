set(PROJECT_NAME BessApp)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
"settings/themes.h"
"settings/settings.h"
"settings/viewport_theme.h"
"pages/page.h"
"pages/main_page/main_page.h"
"pages/main_page/scene_driver.h"
"pages/main_page/main_page_state.h"
"pages/main_page/cmds/add_comp_cmd.h"
"pages/main_page/cmds/delete_comp_cmd.h"
"pages/main_page/cmds/update_value_cmd.h"
"pages/main_page/scene_components/input_scene_component.h"
"pages/main_page/scene_components/group_scene_component.h"
"pages/main_page/scene_components/slot_scene_component.h"
"pages/main_page/scene_components/sim_scene_component.h"
"pages/main_page/scene_components/non_sim_scene_component.h"
"pages/main_page/scene_components/connection_scene_component.h"
"pages/main_page/scene_components/conn_joint_scene_component.h"
# "pages/start_page/start_page.h"
"app_types.h"
"args_parser.h"
"crash_detail_helper.h"
"application.h"
"assets.h"
"project_file.h"
"window.h"
"application_state.h"
"events/application_event.h"
"asset_manager/asset_loader.h"
"asset_manager/asset_manager.h"
"asset_manager/asset_id.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
"settings/viewport_theme.cpp"
"settings/themes.cpp"
"settings/settings.cpp"
"pages/page.cpp"
"pages/main_page/main_page_state.cpp"
"pages/main_page/main_page.cpp"
"pages/main_page/scene_driver.cpp"
# "pages/start_page/start_page.cpp"
"pages/main_page/scene_components/sim_scene_component.cpp"
"pages/main_page/scene_components/group_scene_component.cpp"
"pages/main_page/scene_components/non_sim_scene_component.cpp"
"pages/main_page/scene_components/input_scene_component.cpp"
"pages/main_page/scene_components/slot_scene_component.cpp"
"pages/main_page/scene_components/connection_scene_component.cpp"
"pages/main_page/scene_components/conn_joint_scene_component.cpp"
"application_state.cpp"
"application.cpp"
"window.cpp"
"project_file.cpp"
"events/application_event.cpp"
"asset_manager/asset_manager.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PUBLIC common BessExtDeps BessJson BessSimEngine BessVulkan BessPluginManager EventSystem BessScene BessCmdSystem BessUI)
add_dependencies(${PROJECT_NAME} common BessExtDeps BessJson BessSimEngine BessVulkan BessPluginManager BessScene BessCmdSystem BessUI)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/src/Bess/"
	"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
	"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
	"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
	"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
	"${CMAKE_SOURCE_DIR}/src/CommandSystem/include"
	"${CMAKE_SOURCE_DIR}/src/BessJson/include"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

