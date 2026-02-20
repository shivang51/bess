set(PROJECT_NAME EventSystem)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(EXT_OTHERS_SRC
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_OTHERS_SRC}
)

set(Header_Files
	"include/event_dispatcher.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
	"src/event_dispatcher.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
		${EXT_OTHERS_SRC}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include"
)


target_link_libraries(${PROJECT_NAME} PUBLIC 
)

target_include_directories(${PROJECT_NAME} PUBLIC 
	"${CMAKE_SOURCE_DIR}/external/spdlog/include"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)
