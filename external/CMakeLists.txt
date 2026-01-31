set(PROJECT_NAME BessExtDeps)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

file(GLOB EXT_IMGUI_SRC
	"${CMAKE_SOURCE_DIR}/external/imgui/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implot/*.cpp"
)
source_group("external/imgui" FILES ${EXT_IMGUI_SRC})

file(GLOB EXT_LIB_TESS2
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
)

set(EXT_OTHERS_SRC
	"${CMAKE_SOURCE_DIR}/external/stb_image/src/stb_image.cpp"
	"${CMAKE_SOURCE_DIR}/external/tinyfiledialogs/tinyfiledialogs.c"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
    ${EXT_LIB_TESS2}
)

add_library(${PROJECT_NAME} STATIC)

target_sources(${PROJECT_NAME} PUBLIC ${External})

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(VULKAN REQUIRED vulkan)
find_package(X11 REQUIRED)
find_package(PNG REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC
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
		${GLFW_INCLUDE_DIRS}
		${VULKAN_INCLUDE_DIRS}
		${X11_INCLUDE_DIR}
		${PNG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC 
	${PNG_LIBRARIES}
	${GLFW_LIBRARIES}
	${X11_LIBRARIES}
	${VULKAN_LIBRARIES}
)

if("${BUILD_ARCH}" STREQUAL "x64" AND "${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
		target_link_libraries(${PROJECT_NAME} PUBLIC 
        "gdi32;"
        "freetype;"
    )

		target_link_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_SOURCE_DIR}/external/freetype/libs/windows"
    )

else()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
			target_link_libraries(${PROJECT_NAME} PUBLIC stdc++exp)
    else()
			target_link_libraries(${PROJECT_NAME} PUBLIC stdc++_libbacktrace)
    endif()

		target_link_libraries(${PROJECT_NAME} PUBLIC 
        "freetype;"
    )
endif()


