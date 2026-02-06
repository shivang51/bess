set(PROJECT_NAME "BessCmdSystem")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


set(Header_Files
	"include/command.h"
	"include/command_system.h"
	"include/cmds/update_vec_cmd.h"
)


source_group("include/command_system" FILES ${Header_Files})

set(Source_Files
	"src/command_system.cpp"
)


add_library(${PROJECT_NAME} STATIC ${Header_Files} ${Source_Files})
add_dependencies(${PROJECT_NAME} BessExtDeps BessSimEngine BessScene)

target_link_libraries(${PROJECT_NAME} PUBLIC BessExtDeps BessSimEngine BessScene)

target_include_directories(${PROJECT_NAME} PUBLIC 
	${CMAKE_CURRENT_SOURCE_DIR}/include)


target_include_directories(${PROJECT_NAME} PRIVATE 
	${CMAKE_SOURCE_DIR}/src/Bess/scene
	${CMAKE_SOURCE_DIR}/src/BessSimEngine/include
)
