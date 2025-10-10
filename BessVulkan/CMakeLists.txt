set(PROJECT_NAME BessVulkan)

# Header files to move to BessVulkan
set(Header_Files
    "include/vulkan_core.h"
    "include/device.h"
    "include/swapchain.h"
    "include/command_buffer.h"
    "include/vulkan_shader.h"
    "include/vulkan_texture.h"
    "include/vulkan_subtexture.h"
    "include/vulkan_render_pass.h"
    "include/vulkan_image_view.h"
    "include/vulkan_offscreen_render_pass.h"
    "include/vulkan_postprocess_pipeline.h"
    "include/primitive_vertex.h"
)

source_group("include" FILES ${Header_Files})

# Source files to move to BessVulkan
set(Source_Files
    "src/vulkan_core.cpp"
    "src/device.cpp"
    "src/swapchain.cpp"
    "src/command_buffer.cpp"
    "src/vulkan_shader.cpp"
    "src/vulkan_texture.cpp"
    "src/vulkan_subtexture.cpp"
    "src/vulkan_render_pass.cpp"
    "src/vulkan_image_view.cpp"
    "src/vulkan_offscreen_render_pass.cpp"
    "src/vulkan_postprocess_pipeline.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/BessSimEngine/include"
    "${CMAKE_SOURCE_DIR}/external/imgui"
    "${CMAKE_SOURCE_DIR}/external/implot"
    "${CMAKE_SOURCE_DIR}/external/freetype/include"
    "${CMAKE_SOURCE_DIR}/external/imgui/backends"
    "${CMAKE_SOURCE_DIR}/external/glm"
    "${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
    "${CMAKE_SOURCE_DIR}/external/stb_image/include"
    "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
    "${CMAKE_SOURCE_DIR}/external/entt/include"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
)

# Include directories for the library itself
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(VULKAN REQUIRED vulkan)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${PNG_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${X11_LIBRARIES}
    ${VULKAN_LIBRARIES}
)

# Include directories for dependencies
target_include_directories(${PROJECT_NAME} PUBLIC 
    ${PNG_INCLUDE_DIRS}
    ${VULKAN_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

# Set C++ standard
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
