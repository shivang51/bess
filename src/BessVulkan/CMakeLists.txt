set(PROJECT_NAME BessVulkan)

set(EXT_OTHERS_SRC
	"${CMAKE_SOURCE_DIR}/external/stb_image/src/stb_image.cpp"
	"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_OTHERS_SRC}
)

set(Header_Files
    "include/vulkan_core.h"
    "include/device.h"
    "include/log.h"
    "include/swapchain.h"
    "include/command_buffer.h"
    "include/vulkan_texture.h"
    "include/vulkan_subtexture.h"
    "include/vulkan_render_pass.h"
    "include/vulkan_image_view.h"
    "include/vulkan_offscreen_render_pass.h"
    "include/vulkan_postprocess_pipeline.h"
    "include/primitive_vertex.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/command_buffer.cpp"
    "src/device.cpp"
    "src/log.cpp"
    "src/swapchain.cpp"
    "src/vulkan_core.cpp"
    "src/vulkan_image_view.cpp"
    "src/vulkan_offscreen_render_pass.cpp"
    "src/vulkan_postprocess_pipeline.cpp"
    "src/vulkan_render_pass.cpp"
    "src/vulkan_texture.cpp"
    "src/vulkan_subtexture.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
		${EXT_OTHERS_SRC}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/glm"
    "${CMAKE_SOURCE_DIR}/external/stb_image/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
)


find_package(PkgConfig REQUIRED)
pkg_check_modules(VULKAN REQUIRED vulkan)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${VULKAN_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PUBLIC 
    ${VULKAN_INCLUDE_DIRS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
