set(PROJECT_NAME BessJson)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
	include/bess_json/bess_json.h
	include/bess_json/json_converters.h
)

source_group("include/bess_json" FILES ${Header_Files})

set(Source_Files
	src/json_converters.cpp
)

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${External}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
		"${CMAKE_SOURCE_DIR}/external/glm/"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

