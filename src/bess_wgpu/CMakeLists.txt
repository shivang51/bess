set(PROJECT_NAME BessWgpu)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
    "include/bess_wgpu/wgpu_renderer_2d.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/wgpu_renderer_2d.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    BessCore
)

if(TARGET dawn::webgpu_dawn)
    target_link_libraries(${PROJECT_NAME} PUBLIC dawn::webgpu_dawn)
elseif(TARGET webgpu_dawn)
    target_link_libraries(${PROJECT_NAME} PUBLIC webgpu_dawn)
else()
    message(FATAL_ERROR "Dawn target webgpu_dawn was not created")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
