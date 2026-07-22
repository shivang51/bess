set(PROJECT_NAME UICore)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	include/behaviors/pressable.h
	include/controls/basic_widgets.h
	include/controls/dock_drop.h
	include/controls/dock_space.h
	include/controls/menu_bar.h
	include/controls/scroll_view.h
	include/controls/tab_bar.h
	include/demo_view.h
	include/dock.h
	include/layout.h
	include/models/signal.h
	include/models/dock_model.h
	include/models/menu_model.h
	include/models/tab_model.h
	include/ui_event.h
	include/ui_composer.h
	include/ui_painter.h
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
	src/basic_widgets.cpp
	src/demo_view.cpp
	src/dock.cpp
	src/dock_drop.cpp
	src/dock_model.cpp
	src/dock_space.cpp
	src/menu_model.cpp
	src/menu_bar.cpp
	src/layout.cpp
	src/pressable.cpp
	src/scroll_view.cpp
	src/tab_bar.cpp
	src/ui_painter.cpp
	src/ui_composer.cpp
	src/widget_tree.cpp
	src/ui_style.cpp
	src/ui_target.cpp
	src/ui_view.cpp
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
