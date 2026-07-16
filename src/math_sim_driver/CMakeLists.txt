set(PROJECT_NAME MathSimDriver)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
    "include/math_sim_driver.h"
)

set(Source_Files
    "src/math_sim_driver.cpp"
)

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PRIVATE
    common
    BessSimEngine
    BessSimDriver
    BessJson
    BessExtDeps
)

add_dependencies(${PROJECT_NAME}
    common
    BessSimEngine
    BessSimDriver
    BessJson
    BessExtDeps
)
