set(PROJECT_NAME "BessCmdSystem")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(Header_Files
	"include/command.h"
	"include/macro_command.h"
	"include/command_system.h"
)


source_group("include/command_system" FILES ${Header_Files})

set(Source_Files
	"src/command_system.cpp"
	"src/command.cpp"
	"src/macro_command.cpp"
)


add_library(${PROJECT_NAME} STATIC ${Header_Files} ${Source_Files})
add_dependencies(${PROJECT_NAME} BessJson BessExtDeps BessSimEngine common)

target_link_libraries(${PROJECT_NAME} PUBLIC BessJson BessExtDeps BessSimEngine common)

target_include_directories(${PROJECT_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include
)


target_include_directories(${PROJECT_NAME} PRIVATE 
	${CMAKE_SOURCE_DIR}/src/Bess/application/
	${CMAKE_SOURCE_DIR}/src/bess_core/include/
	${CMAKE_SOURCE_DIR}/src/sim_engine/include/
	${CMAKE_SOURCE_DIR}/src/sim_driver/include/
	${CMAKE_SOURCE_DIR}/src/event_system/include
)
