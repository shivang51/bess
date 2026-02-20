set(PROJECT_NAME common)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Header_Files
"include/common/helpers.h"
"include/common/bind_helpers.h"
"include/common/digital_state.h"
"include/common/bess_assert.h"
"include/common/bess_uuid.h"
"include/common/logger.h"
"include/common/ui_log_sink.h"
)

source_group("include/common" FILES ${Header_Files})

set(Source_Files
	"src/helpers.cpp"
	"src/bess_uuid.cpp"
	"src/logger.cpp"
)

source_group("src/common" FILES ${Source_Files})

set(ALL_FILES
		${Header_Files}
		${Source_Files}
		${External}
)

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE BessJson BessExtDeps)
add_dependencies(${PROJECT_NAME} BessJson)

target_include_directories(${PROJECT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/common
)

if(MSVC)
		target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)


