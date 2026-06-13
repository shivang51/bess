set(PROJECT_NAME BessScene)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"scene.h"
	"scene_ser_reg.h"
	"camera.h"
	"scene_draw_context.h"
	"scene_events.h"
	"scene_serializer.h"
	"scene_state/scene_state.h"
	"scene_state/components/scene_component_types.h"
	"scene_state/components/scene_component.h"
	"scene_state/components/behaviours/drag_behaviour.h"
	"scene_state/components/behaviours/mouse_behaviour.h"
	"schematic_diagram.h"
	"scene_layer.h"
	"scene_event.h"
	"scene_event_builder.h"
	"scene_widgets.h"
	"scene_types.h"
	"layers/grid_layer.h"
	"layers/components_layer.h"
	"layers/hover_layer.h"
	"layers/interaction_layer.h"
	"layers/overlay_layer.h"
	"layers/scene_widgets_layer.h"
)

source_group("include/scene" FILES ${Header_Files})

set(Source_Files
	"scene_serializer.cpp"
	"scene_ser_reg.cpp"
	"scene_state/scene_state.cpp"
	"scene_state/components/scene_component_types.cpp"
	"scene_state/components/scene_component.cpp"
	"scene_event_builder.cpp"
	"scene_widgets.cpp"
	"scene.cpp"
	"camera.cpp"
	"schematic_diagram.cpp"
	"layers/grid_layer.cpp"
	"layers/components_layer.cpp"
	"layers/hover_layer.cpp"
	"layers/interaction_layer.cpp"
	"layers/overlay_layer.cpp"
	"layers/scene_widgets_layer.cpp"
)

source_group("src/scene" FILES ${Source_Files})

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${EXT_OTHERS_SRC}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/src/bess/"
		"${CMAKE_SOURCE_DIR}/src/bess/application"
)

target_link_libraries(${PROJECT_NAME} PUBLIC BessSimEngine  BessPluginManager EventSystem BessJson common BessSimDriver DigitalSimDriver)
add_dependencies(${PROJECT_NAME} BessSimEngine  BessPluginManager EventSystem BessJson common BessSimDriver DigitalSimDriver)
target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

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
# if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
#     target_precompile_headers(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:"${CMAKE_CURRENT_SOURCE_DIR}/scene_pch.h">)
# endif()
