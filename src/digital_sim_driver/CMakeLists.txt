set(PROJECT_NAME DigitalSimDriver)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	"include/dig_sim_driver.h"
	"include/dig_module_def.h"
)

set(Source_Files
	"src/dig_sim_driver.cpp"
	"src/dig_module_def.cpp"
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
