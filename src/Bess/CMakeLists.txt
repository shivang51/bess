set(PROJECT_NAME Bess)
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_BUILD_RPATH "\$ORIGIN")
set(CMAKE_INSTALL_RPATH "\$ORIGIN")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory("scene")
add_subdirectory("ui")

file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

file(GLOB EXT_IMGUI_SRC
	"${CMAKE_SOURCE_DIR}/external/imgui/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implot/*.cpp"
)
source_group("external/imgui" FILES ${EXT_IMGUI_SRC})

set(EXT_OTHERS_SRC
	"${CMAKE_SOURCE_DIR}/external/stb_image/src/stb_image.cpp"
    "${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
)

set(Header_Files
"settings/themes.h"
"settings/settings.h"
"settings/viewport_theme.h"
"pages/page.h"
"pages/main_page/main_page.h"
"pages/main_page/scene_driver.h"
"pages/main_page/main_page_state.h"
"pages/start_page/start_page.h"
"pages/page_identifier.h"
"common/helpers.h"
"common/bind_helpers.h"
"common/digital_state.h"
"common/log.h"
"application/types.h"
"application/args_parser.h"
"application/crash_detail_helper.h"
"application/application.h"
"application/assets.h"
"application/project_file.h"
"application/window.h"
"application/application_state.h"
"events/application_event.h"
"events/scene_events.h"
"asset_manager/asset_loader.h"
"asset_manager/asset_manager.h"
"asset_manager/asset_id.h"
)

source_group("include" FILES ${Header_Files})

set(Source_Files
"settings/viewport_theme.cpp"
"settings/themes.cpp"
"settings/settings.cpp"
"pages/page.cpp"
"pages/main_page/main_page_state.cpp"
"pages/main_page/main_page.cpp"
"pages/main_page/scene_driver.cpp"
"pages/start_page/start_page.cpp"
"common/helpers.cpp"
"application/application_state.cpp"
"application/application.cpp"
"application/window.cpp"
"application/main.cpp"
"application/project_file.cpp"
"events/application_event.cpp"
"asset_manager/asset_manager.cpp"
)

source_group("src" FILES ${Source_Files})

set(ALL_FILES
    ${External}
    ${Header_Files}
    ${Source_Files}
)

add_executable(${PROJECT_NAME} ${ALL_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE BessSimEngine BessVulkan BessPluginManager EventSystem BessScene BessUI)
add_dependencies(${PROJECT_NAME} BessSimEngine BessVulkan BessPluginManager BessScene BessUI)

if("${BUILD_ARCH}" STREQUAL "x64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "gdi32;"
        "freetype;"
    )

    target_link_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_SOURCE_DIR}/external/freetype/libs/windows"
    )

else()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
	    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++exp)
    else()
		target_link_libraries(${PROJECT_NAME} PRIVATE stdc++_libbacktrace)
    endif()

    set(ADDITIONAL_LIBRARY_DEPENDENCIES
        "freetype;"
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(ASSETS_DST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets")

    file(MAKE_DIRECTORY "${ASSETS_DST_DIR}")

    add_custom_command(
				TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "${ASSETS_DST_DIR}"
        COMMENT "Copying assets to build directory"
    )
endif()

if("${BUILD_ARCH}" STREQUAL "x64")
	target_include_directories(${PROJECT_NAME} PRIVATE
			"${CMAKE_CURRENT_SOURCE_DIR}"
			"${CMAKE_SOURCE_DIR}/src/BessSimEngine/include"
			"${CMAKE_SOURCE_DIR}/src/BessVulkan/include"
			"${CMAKE_SOURCE_DIR}/src/BessPluginManager/include"
			"${CMAKE_SOURCE_DIR}/src/EventSystem/include"
			"${CMAKE_SOURCE_DIR}/external/imgui"
			"${CMAKE_SOURCE_DIR}/external/implot"
			"${CMAKE_SOURCE_DIR}/external/freetype/include"
			"${CMAKE_SOURCE_DIR}/external/imgui/backends"
			"${CMAKE_SOURCE_DIR}/external/glm"
			"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
			"${CMAKE_SOURCE_DIR}/external/stb_image/include"
			"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs"
			"${CMAKE_SOURCE_DIR}/external/spdlog/include"
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
	)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(VULKAN REQUIRED vulkan)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC 
	${PNG_LIBRARIES}
	${GLFW_LIBRARIES}
	${X11_LIBRARIES}
	${VULKAN_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PUBLIC 
	${PNG_INCLUDE_DIRS}
	${VULKAN_INCLUDE_DIRS}
	${GLFW_INCLUDE_DIRS}
)


file(GLOB libtess2_src
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
)
add_library(libtess2_obj OBJECT ${libtess2_src})
target_include_directories(libtess2_obj PUBLIC 
			"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:libtess2_obj>)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8" "/Zc:__cplusplus")
endif()

