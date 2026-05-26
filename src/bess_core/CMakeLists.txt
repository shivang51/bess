set(PROJECT_NAME BessCore)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"include/bess_core/g_app_context.h"
	"include/bess_core/project_context.h"
	"include/bess_core/scene_driver.h"
	"include/bess_core/copy_paste_service.h"
	"include/bess_core/connection_service.h"

	"include/bess_core/renderer/texture.h"
	"include/bess_core/renderer/renderer.h"
	"include/bess_core/renderer/renderer_types.h"
)

source_group("include/bess_core" FILES ${Header_Files})

set(Source_Files
	"src/bess_core/g_app_context.cpp"
	"src/bess_core/project_context.cpp"
	"src/bess_core/scene_driver.cpp"
	"src/bess_core/copy_paste_service.cpp"
	"src/bess_core/connection_service.cpp"
	
	"src/bess_core/renderer/texture.cpp"
	"src/bess_core/renderer/renderer.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
	${Header_Files}
	${Source_Files}
)

add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE BESS_EXPORTS)
target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PUBLIC common BessJson BessExtDeps)

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
	"${CMAKE_SOURCE_DIR}/src/bess_vulkan/include"
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
