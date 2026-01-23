set(PROJECT_NAME BessUI)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(EXT_OTHERS_SRC
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
		${EXT_OTHERS_SRC}
)

set(Header_Files
	"icons/ComponentIcons.h"
	"icons/CodIcons.h"
	"icons/FontAwesomeIcons.h"
	"icons/MaterialIcons.h"
	"widgets/m_widgets.h"
	"ui.h"
	"ui_hook.h"
	"ui_main/properties_panel.h"
	"ui_main/settings_window.h"
	"ui_main/graph_view_window.h"
	"ui_main/project_settings_window.h"
	"ui_main/component_explorer.h"
	"ui_main/truth_table_window.h"
	"ui_main/scene_export_window.h"
	"ui_main/ui_main.h"
	"ui_main/popups.h"
	"ui_main/dialogs.h"
	"ui_main/project_explorer.h"
	"ui_main/project_explorer_state.h"
	"ui_main/scene_viewport.h"
)

source_group("include/ui" FILES ${Header_Files})

set(Source_Files
  "ui.cpp"
	"ui_hook.cpp"
	"widgets/m_widgets.cpp"
	"ui_main/component_explorer.cpp"
	"ui_main/properties_panel.cpp"
	"ui_main/project_explorer.cpp"
	"ui_main/project_explorer_state.cpp"
	"ui_main/project_settings_window.cpp"
	"ui_main/dialogs.cpp"
	"ui_main/ui_main.cpp"
	"ui_main/settings_window.cpp"
	"ui_main/scene_export_window.cpp"
	"ui_main/scene_viewport.cpp"
	"ui_main/truth_table_window.cpp"
	"ui_main/popups.cpp"
	"ui_main/graph_view_window.cpp"
)

source_group("src/ui" FILES ${Source_Files})

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${EXT_OTHERS_SRC}
		${BESS_UI_SOURCES}
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
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
)

target_link_libraries(${PROJECT_NAME} PUBLIC BessSimEngine BessVulkan BessPluginManager EventSystem BessScene)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

