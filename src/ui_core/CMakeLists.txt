set(PROJECT_NAME UICore)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	include/controls/drag_drop_widgets.h
	include/controls/reorderable_list.h
	include/behaviors/pressable.h
	include/controls/action_button.h
	include/controls/basic_widgets.h
	include/controls/card.h
	include/controls/focus_scope.h
	include/controls/image.h
	include/controls/list_view.h
	include/controls/numeric_input.h
	include/controls/render_view.h
	include/controls/scene_view.h
	include/controls/dock_drop.h
	include/controls/dock_space.h
	include/controls/menu_bar.h
	include/controls/popup_controls.h
	include/controls/scroll_view.h
	include/controls/tab_bar.h
	include/controls/text_box.h
	include/controls/tree_node.h
	include/controls/value_controls.h
	include/demo_view.h
	include/drag_drop.h
	include/dock.h
	include/layout.h
	include/models/signal.h
	include/models/action_registry.h
	include/models/dock_model.h
	include/models/dropdown_model.h
	include/models/menu_model.h
	include/models/tab_model.h
	include/models/text_edit_model.h
	include/models/value_models.h
	include/popup.h
	include/render_surface.h
	include/ui_event.h
	include/ui_composer.h
	include/ui_painter.h
	include/ui_platform_services.h
	include/widget_tree.h
	include/ui_style.h
	include/ui_target.h
	include/ui_types.h
	include/ui_view.h
	include/widget.h
	include/widget_ref.h
	
	include/ui_core/ui_core.h
)

source_group("include/ui_core" FILES ${Header_Files})

set(Source_Files
	src/drag_drop_widgets.cpp
	src/reorderable_list.cpp
	src/action_button.cpp
	src/basic_widgets.cpp
	src/card.cpp
	src/action_registry.cpp
	src/focus_scope.cpp
	src/image.cpp
	src/list_view.cpp
	src/numeric_input.cpp
	src/render_surface.cpp
	src/render_view.cpp
	src/scene_view.cpp
	src/demo_view.cpp
	src/dock.cpp
	src/drag_drop.cpp
	src/dock_drop.cpp
	src/dock_model.cpp
	src/dock_space.cpp
	src/dropdown_model.cpp
	src/menu_model.cpp
	src/menu_bar.cpp
	src/layout.cpp
	src/pressable.cpp
	src/popup.cpp
	src/popup_controls.cpp
	src/scroll_view.cpp
	src/tab_bar.cpp
	src/text_box.cpp
	src/text_edit_model.cpp
	src/tree_node.cpp
	src/ui_painter.cpp
	src/ui_platform_services.cpp
	src/ui_composer.cpp
	src/widget_tree.cpp
	src/ui_style.cpp
	src/ui_target.cpp
	src/ui_view.cpp
	src/value_controls.cpp
	src/widget.cpp
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
	${Header_Files}
	${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PUBLIC common BessJson BessExtDeps EventSystem BessCore)

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include/ui_core"
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
