#pragma once

// The loop is there to make it behave like a single statement in all contexts.
#ifdef DEBUG
    #include <cassert>
    #include <format>
    #include <iostream>
    #include <stacktrace>

    #define BESS_ASSERT(expr, ...)                                             \
        do {                                                                   \
            if (!(expr)) {                                                     \
                std::cerr << "Assertion failed: (" << #expr << ")"             \
                          << "  File: " << __FILE__ << ":"                     \
                          << "  Line: " << __LINE__ << "\n"                    \
                          << "  Message: " << std::format(__VA_ARGS__)         \
                          << std::endl;                                        \
                std::cerr.flush();                                             \
                const std::stacktrace st = std::stacktrace::current();         \
                std::cout << st << std::endl;                                  \
                std::abort();                                                  \
            }                                                                  \
        } while (false)
#else
    #define BESS_ASSERT(expr, msg) ((void)0)
#endif
