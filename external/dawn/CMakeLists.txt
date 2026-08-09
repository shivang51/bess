
add_library(dawn_headers INTERFACE)
target_include_directories(dawn_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(dawn SHARED IMPORTED GLOBAL)
add_library(dawn::dawn ALIAS dawn)

target_link_libraries(dawn INTERFACE dawn_headers)

if(WIN32)
    set_target_properties(dawn PROPERTIES
        IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/webgpu_dawn.lib"
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/bin/webgpu_dawn.dll"
    )

    # The prebuilt Dawn DLL enables its D3D12 backend, which loads the DirectX
    # Shader Compiler at runtime. Fetch a pinned Microsoft redistributable so a
    # packaged Bess build does not depend on DXC being installed on the target
    # machine.
    FetchContent_Declare(
        dawn_dxc_runtime
        URL
            "https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.dxc/1.8.2505.32/microsoft.direct3d.dxc.1.8.2505.32.nupkg"
        URL_HASH
            "SHA256=c6e82b70c14552f1dd58e4a79c93eeab1567eeb0a9ee63a51564c410429bce3e"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(dawn_dxc_runtime)

    set(DAWN_DXC_RUNTIME_DIR
        "${dawn_dxc_runtime_SOURCE_DIR}/build/native/bin/x64"
    )

    add_library(dawn_dxcompiler SHARED IMPORTED GLOBAL)
    set_target_properties(dawn_dxcompiler PROPERTIES
        IMPORTED_LOCATION "${DAWN_DXC_RUNTIME_DIR}/dxcompiler.dll"
    )

    add_library(dawn_dxil SHARED IMPORTED GLOBAL)
    set_target_properties(dawn_dxil PROPERTIES
        IMPORTED_LOCATION "${DAWN_DXC_RUNTIME_DIR}/dxil.dll"
    )
else()
    set_target_properties(dawn PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib64/libwebgpu_dawn.so"
    )
endif()
