#pragma once

#ifdef _WIN32
#ifndef BESS_API 
    #ifdef BESS_EXPORTS
        #define BESS_API __declspec(dllexport)
    #else
        #define BESS_API __declspec(dllimport)
    #endif
#endif
#else
    #define BESS_API
#endif
