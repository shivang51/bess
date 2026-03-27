set(PROJECT_NAME bverilog)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
    "include/bess_api.h"
    "include/bverilog/types.h"
    "include/bverilog/yosys_json_parser.h"
    "include/bverilog/yosys_runner.h"
    "include/bverilog/sim_engine_importer.h"
)
source_group("include" FILES ${Header_Files})

set(Source_Files
    "src/yosys_json_parser.cpp"
    "src/yosys_runner.cpp"
    "src/sim_engine_importer.cpp"
)
source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC BessJson common BessSimEngine)
add_dependencies(${PROJECT_NAME} BessJson common BessSimEngine)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
    "${CMAKE_SOURCE_DIR}/src/common/include"
    "${CMAKE_SOURCE_DIR}/src/bess_json/include"
    "${CMAKE_SOURCE_DIR}/src/sim_engine/include"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif()
