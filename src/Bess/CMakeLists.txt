set(PROJECT_NAME Bess)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_BUILD_RPATH "\$ORIGIN")
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory("scene")
add_subdirectory("ui")

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
"pages/main_page/cmds/update_vec_cmd.h"
"pages/main_page/scene_components/input_scene_component.h"
"pages/main_page/scene_components/slot_scene_component.h"
"pages/main_page/scene_components/sim_scene_component.h"
"pages/main_page/scene_components/non_sim_scene_component.h"
"pages/main_page/scene_components/connection_scene_component.h"
"pages/main_page/scene_components/conn_joint_scene_component.h"
"pages/start_page/start_page.h"
"common/helpers.h"
"common/bind_helpers.h"
"common/digital_state.h"
"common/log.h"
"application/types.h"
"application/args_parser.h"
"application/crash_detail_helper.h"
"application/application.h"
"application/assets.h"
"application/project_file.h"
"application/window.h"
"application/application_state.h"
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
"pages/start_page/start_page.cpp"
"pages/main_page/scene_components/sim_scene_component.cpp"
"pages/main_page/scene_components/non_sim_scene_component.cpp"
"pages/main_page/scene_components/input_scene_component.cpp"
"pages/main_page/scene_components/slot_scene_component.cpp"
"pages/main_page/scene_components/connection_scene_component.cpp"
"pages/main_page/scene_components/conn_joint_scene_component.cpp"
"common/helpers.cpp"
"application/application_state.cpp"
"application/application.cpp"
"application/window.cpp"
"application/main.cpp"
"application/project_file.cpp"
"events/application_event.cpp"
"asset_manager/asset_manager.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_executable(${PROJECT_NAME} ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE BessExtDeps BessJson BessSimEngine BessVulkan BessPluginManager EventSystem BessScene BessCmdSystem BessUI)
add_dependencies(${PROJECT_NAME} BessExtDeps besspybindings BessJson BessSimEngine BessVulkan BessPluginManager BessScene BessCmdSystem BessUI)

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ASSETS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")
		set(BESSPLUG_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/bindings/bessplug")
		set(PLUGINS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "${ASSETS_DST_DIR}"
        COMMENT "Copying assets to build directory"
    )

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
					"${CMAKE_SOURCE_DIR}/plugins" "${PLUGINS_DST_DIR}"
        COMMENT "Copying plugins to build directory"
    )

		add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_directory
				"${CMAKE_SOURCE_DIR}/src/bessplug/py" "${BESSPLUG_DST_DIR}"
				COMMENT "Copying bindings to build directory"
		)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
	"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
	"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
	"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
	"${CMAKE_SOURCE_DIR}/src/CommandSystem/include"
	"${CMAKE_SOURCE_DIR}/src/BessJson/include"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

