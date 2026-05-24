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
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
    ${EXT_LIB_TESS2}
)

add_library(${PROJECT_NAME} OBJECT)

target_sources(${PROJECT_NAME} PRIVATE ${External})
include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_Declare(
    libpng
    GIT_REPOSITORY "https://github.com/pnggroup/libpng.git"
    GIT_TAG "v1.6.55"
)

set(PNG_TESTS OFF CACHE BOOL "Build libpng tests" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "Build libpng tools" FORCE)

FetchContent_MakeAvailable(glfw libpng)

find_package(Vulkan REQUIRED)
find_package(PNG REQUIRED)

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	find_package(X11 REQUIRED)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/external/imgui"
		"${CMAKE_SOURCE_DIR}/external/portable-file-dialogs"
		"${CMAKE_SOURCE_DIR}/external/implot"
		"${CMAKE_SOURCE_DIR}/external/freetype/include"
		"${CMAKE_SOURCE_DIR}/external/imgui/backends"
		"${CMAKE_SOURCE_DIR}/external/glm"
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
		"${CMAKE_SOURCE_DIR}/external/stb_image/include"
		"${CMAKE_SOURCE_DIR}/external/spdlog/include"
		"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
		${GLFW_INCLUDE_DIRS}
		${VULKAN_INCLUDE_DIRS}
		${X11_INCLUDE_DIR}
		${PNG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC 
	PNG::PNG
	glfw
	${X11_LIBRARIES}
	Vulkan::Vulkan
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


