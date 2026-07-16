set(PROJECT_NAME BessSimDriver)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"include/sim_driver/comp_def.h"
	"include/sim_driver/event_based_sim_driver.h"
	"include/sim_driver/sim_driver.h"
	"include/net/net.h" 
)

set(Source_Files
    "src/sim_driver.cpp"
    "src/event_based_sim_driver.cpp"
    "src/net/net.cpp" 
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
	BessJson
	BessExtDeps
)

add_dependencies(${PROJECT_NAME} 
	common
	BessJson
	BessExtDeps
)
