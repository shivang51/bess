#pragma once

#include "bess_runtime_export.h"

#if defined(_WIN32)
#    undef BESS_RUNTIME_API
#    define BESS_RUNTIME_API
#    ifdef BESS_RUNTIME_DEPRECATED_EXPORT
#        undef BESS_RUNTIME_DEPRECATED_EXPORT
#        define BESS_RUNTIME_DEPRECATED_EXPORT BESS_RUNTIME_DEPRECATED
#    endif
#    ifdef BESS_RUNTIME_NO_EXPORT
#        undef BESS_RUNTIME_NO_EXPORT
#        define BESS_RUNTIME_NO_EXPORT
#    endif
#    if defined(BessRuntime_EXPORTS)
#        define BESS_DATA_API __declspec(dllexport)
#    else
#        define BESS_DATA_API __declspec(dllimport)
#    endif
#else
#    ifndef BESS_DATA_API
#        define BESS_DATA_API
#    endif
#endif

#ifndef BESS_API
#    define BESS_API BESS_RUNTIME_API
#endif
