#pragma once

#if defined(_WIN32)
    #ifdef BESS_VULKAN_EXPORT
        #define BESS_VULKAN_API __declspec(dllexport)
    #else
        #define BESS_VULKAN_API __declspec(dllimport)
    #endif
#else
    #define BESS_VULKAN_API __attribute__((visibility("default")))
#endif
