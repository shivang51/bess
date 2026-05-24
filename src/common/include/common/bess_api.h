#pragma once

#ifdef _WIN32
    #ifndef BESS_API
        #ifdef BESS_EXPORTS
            #define BESS_API __declspec(dllexport)
        #else
            #define BESS_API __declspec(dllimport)
        #endif
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define BESS_API __attribute__((visibility("default")))
#else
    #define BESS_API
#endif
