set(PROJECT_NAME BessWgpu)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
    "include/bess_wgpu/piplines/pipeline.h"
    "include/bess_wgpu/piplines/path_pipeline.h"
    "include/bess_wgpu/piplines/primitive_pipeline.h"
    "include/bess_wgpu/piplines/shadow_pipeline.h"
    "include/bess_wgpu/piplines/custom_quad_pipeline.h"
    "include/bess_wgpu/text/bitmap_text_pipeline.h"
    "include/bess_wgpu/text/msdf_text_pipeline.h"
    "include/bess_wgpu/text/text_metrics.h"
    "include/bess_wgpu/wgpu_renderer_2d.h"
    "include/bess_wgpu/wgpu_render_target_2d.h"
    "include/bess_wgpu/wgpu_shader.h"
    "include/bess_wgpu/wgpu_texture.h"
    "include/bess_wgpu/shaders/path_shader.h"
    "include/bess_wgpu/shaders/primitive_shader.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/path_baker.cpp"
    "src/piplines/path_pipeline.cpp"
    "src/piplines/primitive_pipeline.cpp"
    "src/piplines/shadow_pipeline.cpp"
    "src/piplines/custom_quad_pipeline.cpp"
    "src/text/bitmap_text_pipeline.cpp"
    "src/text/msdf_text_pipeline.cpp"
    "src/wgpu_renderer_2d.cpp"
    "src/wgpu_render_target_2d.cpp"
    "src/wgpu_renderer_2d_batches.h"
    "src/wgpu_renderer_2d_instances.h"
    "src/wgpu_renderer_2d_readback.h"
    "src/wgpu_renderer_2d_text.h"
    "src/wgpu_shader.cpp"
    "src/wgpu_texture.cpp"
    "src/shaders/path_shader.cpp"
    "src/shaders/primitive_shader.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/external/stb_image/include"
)



target_link_libraries(${PROJECT_NAME} PUBLIC
    BessCore
    glfw3webgpu
		dawn
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
