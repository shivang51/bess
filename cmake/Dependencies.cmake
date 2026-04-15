include_guard(GLOBAL)

include(FetchContent)

option(BESS_FETCH_DEPS "Automatically download missing third-party dependencies" ON)

set(BESS_GLFW_TARGET "" CACHE INTERNAL "Normalized GLFW target")
set(BESS_FREETYPE_TARGET "" CACHE INTERNAL "Normalized Freetype target")
set(BESS_VULKAN_TARGET "" CACHE INTERNAL "Normalized Vulkan target")

function(_bess_set_target var target_name)
    set(${var} "${target_name}" CACHE INTERNAL "" FORCE)
endfunction()

# GLFW
find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
    _bess_set_target(BESS_GLFW_TARGET glfw)
elseif(TARGET glfw3::glfw)
    _bess_set_target(BESS_GLFW_TARGET glfw3::glfw)
elseif(BESS_FETCH_DEPS)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz
    )
    FetchContent_MakeAvailable(glfw)

    if(TARGET glfw)
        _bess_set_target(BESS_GLFW_TARGET glfw)
    elseif(TARGET glfw3::glfw)
        _bess_set_target(BESS_GLFW_TARGET glfw3::glfw)
    endif()
endif()

if(NOT BESS_GLFW_TARGET)
    message(FATAL_ERROR "GLFW not found. Run scripts/bootstrap_deps.sh (Linux/macOS) or scripts/bootstrap_deps.ps1 (Windows), or configure with -DBESS_FETCH_DEPS=ON.")
endif()

# Freetype
find_package(Freetype QUIET)
if(TARGET Freetype::Freetype)
    _bess_set_target(BESS_FREETYPE_TARGET Freetype::Freetype)
elseif(BESS_FETCH_DEPS)
    set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)
    set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)

    FetchContent_Declare(freetype
        URL https://gitlab.freedesktop.org/freetype/freetype/-/archive/VER-2-13-3/freetype-VER-2-13-3.tar.gz
    )
    FetchContent_MakeAvailable(freetype)

    if(TARGET Freetype::Freetype)
        _bess_set_target(BESS_FREETYPE_TARGET Freetype::Freetype)
    elseif(TARGET freetype)
        _bess_set_target(BESS_FREETYPE_TARGET freetype)
    endif()
endif()

if(NOT BESS_FREETYPE_TARGET)
    message(FATAL_ERROR "Freetype not found. Run scripts/bootstrap_deps.sh (Linux/macOS) or scripts/bootstrap_deps.ps1 (Windows), or configure with -DBESS_FETCH_DEPS=ON.")
endif()

# Vulkan / MoltenVK
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        /opt/homebrew
        /usr/local
        /opt/homebrew/opt/molten-vk
        /usr/local/opt/molten-vk
        /opt/homebrew/opt/vulkan-loader
        /usr/local/opt/vulkan-loader
    )
endif()

find_package(Vulkan QUIET)
if(TARGET Vulkan::Vulkan)
    _bess_set_target(BESS_VULKAN_TARGET Vulkan::Vulkan)
else()
    if(APPLE)
        message(FATAL_ERROR
            "Vulkan was not found on macOS. Install MoltenVK and Vulkan loader first. "
            "Run scripts/bootstrap_deps.sh and then reconfigure.")
    elseif(WIN32)
        message(FATAL_ERROR
            "Vulkan was not found on Windows. Install LunarG Vulkan SDK first. "
            "Run scripts/bootstrap_deps.ps1 and then reconfigure.")
    else()
        message(FATAL_ERROR
            "Vulkan was not found on Linux. Install vulkan-loader and headers first. "
            "Run scripts/bootstrap_deps.sh and then reconfigure.")
    endif()
endif()
