set(PROJECT_NAME bverilog)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# IDK why for this target, fmt needs this, 
# but it won't link without
add_compile_definitions(FMT_HEADER_ONLY)

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

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE BessExtDeps BessJson common BessSimEngine BessSimDriver DigitalSimDriver)
add_dependencies(${PROJECT_NAME} BessJson common BessExtDeps BessSimEngine BessSimDriver DigitalSimDriver)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif()
