#pragma once

// The loop is there to make it behave like a single statement in all contexts.
#ifdef DEBUG
    #include <cassert>
    #include <format>
    #include <iostream>
    #include <version>
    #if __has_include(<stacktrace>)
        #include <stacktrace>
    #endif

    // libc++ (macOS) does not implement std::stacktrace yet.
    #if defined(__cpp_lib_stacktrace)
        #define BESS_PRINT_STACKTRACE(stream)                                  \
            stream << std::stacktrace::current() << std::endl
    #else
        #define BESS_PRINT_STACKTRACE(stream)                                  \
            stream << "(stacktrace unavailable)" << std::endl
    #endif

    #define BESS_ASSERT(expr, ...)                                             \
        do {                                                                   \
            if (!(expr)) {                                                     \
                std::cerr << "Assertion failed: (" << #expr << ")"             \
                          << "  File: " << __FILE__ << ":"                     \
                          << "  Line: " << __LINE__ << "\n"                    \
                          << "  Message: " << std::format(__VA_ARGS__)         \
                          << std::endl;                                        \
                std::cerr.flush();                                             \
                BESS_PRINT_STACKTRACE(std::cout);                              \
                std::abort();                                                  \
            }                                                                  \
        } while (false)
#else
    #define BESS_ASSERT(expr, ...)
#endif
