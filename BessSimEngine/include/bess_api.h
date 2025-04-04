#pragma once

#ifdef _WIN32
    #ifdef BESS_EXPORTS
        #define BESS_API __declspec(dllexport)
    #else
        #define BESS_API __declspec(dllimport)
    #endif
#else
    #define BESS_API
#endif
