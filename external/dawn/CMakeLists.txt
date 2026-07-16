
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
else()
    set_target_properties(dawn PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib64/libwebgpu_dawn.so"
    )
endif()

