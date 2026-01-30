set(PROJECT_NAME BessJson)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB EXT_JSON_INCLUDES "${CMAKE_SOURCE_DIR}/external/jsoncpp/include/*.h")
file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

set(External
	 "${EXT_JSON_SRC}"
)

set(Header_Files
	include/bess_json/bess_json.h
	include/bess_json/json_converters.h
)

source_group("include/bess_json" FILES ${Header_Files})

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${External}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

