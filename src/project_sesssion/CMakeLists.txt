set(PROJECT_NAME ProjectSession)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
)

source_group("include/project_session" FILES ${Header_Files})

set(Source_Files
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
	${Header_Files}
	${Source_Files}
)

add_library(${PROJECT_NAME} OBJECT ${ALL_FILES})

target_compile_definitions(${PROJECT_NAME} PRIVATE LOGGER_NAME="${PROJECT_NAME}")

target_link_libraries(${PROJECT_NAME} PUBLIC common BessJson BessExtDeps EventSystem BessCore BessSimEngine)

target_include_directories(${PROJECT_NAME} PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include/project_session"
)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include/"
)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
