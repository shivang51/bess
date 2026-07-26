set(PROJECT_NAME BessExtDeps)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB EXT_JSON_SRC "${CMAKE_SOURCE_DIR}/external/jsoncpp/src/lib_json/*.cpp")
source_group("external/jsoncpp" FILES ${EXT_JSON_SRC})

file(GLOB EXT_LIB_TESS2
	"${CMAKE_SOURCE_DIR}/external/libtess2/Source/*.c"
)

set(EXT_OTHERS_SRC
	"${CMAKE_SOURCE_DIR}/external/stb_image/src/stb_image.cpp"
)
source_group("external/others" FILES ${EXT_OTHERS_SRC})

add_library(${PROJECT_NAME} OBJECT)

add_library(EnTT INTERFACE)
add_library(EnTT::EnTT ALIAS EnTT)
target_include_directories(EnTT SYSTEM INTERFACE
    "${CMAKE_SOURCE_DIR}/external/entt/include"
)

include(FetchContent)

find_package(Freetype REQUIRED)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build GLFW documentation" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build GLFW tests" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate GLFW install rules" FORCE)

FetchContent_Declare(
    libpng
    GIT_REPOSITORY "https://github.com/pnggroup/libpng.git"
    GIT_TAG "v1.6.55"
)

set(PNG_TESTS OFF CACHE BOOL "Build libpng tests" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "Build libpng tools" FORCE)

# Abseil 20260526.0 makes absl::strings depend on itself. Native CMake ignores
# the alias cycle, but vcpkg resolves the alias and rejects it on Windows.
FetchContent_Declare(
    abseil
    GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
    GIT_TAG "20260526.0"
    GIT_SHALLOW TRUE
    PATCH_COMMAND
        "${CMAKE_COMMAND}"
        "-DABSEIL_SOURCE_DIR=<SOURCE_DIR>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/patches/patch_abseil_20260526.cmake"
)

set(ABSL_BUILD_TESTING OFF CACHE BOOL "Disable Abseil tests" FORCE)
set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "Disable Abseil install rules" FORCE)
set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL
    "Build Abseil with the consuming target's C++ standard" FORCE)


# FetchContent_Declare(
#     dawn
#     GIT_REPOSITORY "https://dawn.googlesource.com/dawn"
#     GIT_TAG "chromium/7161"
#     GIT_SUBMODULES ""
#     # Prevents MakeAvailable from automatically calling add_subdirectory()
#     SOURCE_SUBDIR "bypassed_automatic_add_subdirectory"
# )

# set(DAWN_FETCH_DEPENDENCIES ON CACHE BOOL "Fetch Dawn dependencies" FORCE)
# set(FETCHCONTENT_UPDATES_DISCONNECTED_DAWN ON CACHE BOOL
#     "Do not update the pinned Dawn checkout during normal builds" FORCE)
# set(DAWN_ENABLE_INSTALL OFF CACHE BOOL "Disable Dawn install rules" FORCE)
# set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "Disable Dawn sample targets" FORCE)
# set(DAWN_BUILD_TESTS OFF CACHE BOOL "Disable Dawn test targets" FORCE)
# set(TINT_BUILD_TESTS OFF CACHE BOOL "Disable Tint test targets" FORCE)
# set(TINT_BUILD_CMD_TOOLS   OFF CACHE BOOL "Disable building standalone Tint CLI" FORCE)
# set(DAWN_USE_GLFW OFF CACHE BOOL "Disable internal GLFW helper target" FORCE)
#
# if(LINUX)
# 	set(TINT_BUILD_HLSL_WRITER OFF CACHE BOOL "Disable Direct3D shader compilation" FORCE)
# 	set(TINT_BUILD_MSL_WRITER  OFF CACHE BOOL "Disable Metal shader compilation" FORCE)
# endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wno-error=invalid-constexpr>")
endif()

FetchContent_MakeAvailable(glfw libpng abseil)

# add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR})
add_subdirectory(glfw3webgpu)
add_subdirectory(yoga)


if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
	find_package(X11 REQUIRED)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
		"${CMAKE_SOURCE_DIR}/external/imgui"
		"${CMAKE_SOURCE_DIR}/external/portable-file-dialogs"
		"${CMAKE_SOURCE_DIR}/external/implot"
		"${CMAKE_SOURCE_DIR}/external/imgui/backends"
		"${CMAKE_SOURCE_DIR}/external/glm"
		"${CMAKE_SOURCE_DIR}/external/jsoncpp/include"
		"${CMAKE_SOURCE_DIR}/external/stb_image/include"
		"${CMAKE_SOURCE_DIR}/external/spdlog/include"
		"${CMAKE_SOURCE_DIR}/external/libtess2/Include"
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
		IMGUI_IMPL_WEBGPU_BACKEND_DAWN
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	LOGGER_NAME="${PROJECT_NAME}"
)

file(GLOB EXT_IMGUI_SRC
	"${CMAKE_SOURCE_DIR}/external/imgui/backends/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/imgui/*.cpp"
    "${CMAKE_SOURCE_DIR}/external/implot/*.cpp"
)
source_group("external/imgui" FILES ${EXT_IMGUI_SRC})

set(External
    ${EXT_JSON_SRC}
    ${EXT_IMGUI_SRC}
    ${EXT_OTHERS_SRC}
    ${EXT_LIB_TESS2}
)

target_sources(${PROJECT_NAME} PRIVATE ${External})

add_subdirectory(material-color-utilities)
add_subdirectory(dawn)

if(LINUX)
		target_link_libraries(${PROJECT_NAME} PUBLIC 
				X11::X11 
		)
endif()


if(WIN32)
	target_link_libraries(${PROJECT_NAME} PUBLIC gdi32)
else()
    if (${CMAKE_CXX_COMPILER_VERSION} VERSION_GREATER_EQUAL "14")
			target_link_libraries(${PROJECT_NAME} PUBLIC stdc++exp)
    else()
			target_link_libraries(${PROJECT_NAME} PUBLIC stdc++_libbacktrace)
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC 
	png_static
	glfw
	dawn
	material-color-utils
	absl::flat_hash_map
	absl::flat_hash_set
	absl::linked_hash_set
	absl::btree
	absl::raw_hash_set           
	absl::base
	absl::raw_logging_internal
	yogacore
	Freetype::Freetype
)
