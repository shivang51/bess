#pragma once

// This helper sets up detailed crash reporting for Linux systems using stacktrace.
// Only for Debug builds.

#ifndef SETUP_CRASH_DETAIL_HELPER
    #define SETUP_CRASH_DETAIL_HELPER

    #if defined(__linux__) && defined(DEBUG)
        #include <csignal>
        #include <cstdlib>
        #include <iostream>
        #include <stacktrace>

void signalHandler(int sig) {
    std::cerr << "Error: signal " << sig << std::endl;
    const std::stacktrace st = std::stacktrace::current();
    std::cout << st << std::endl;
    exit(1);
}

        #undef SETUP_CRASH_DETAIL_HELPER
        #define SETUP_CRASH_DETAIL_HELPER  \
            struct sigaction sa;           \
            sa.sa_handler = signalHandler; \
            sigemptyset(&sa.sa_mask);      \
            sa.sa_flags = SA_RESTART;      \
            sigaction(SIGSEGV, &sa, nullptr);
    #endif // _LINUX

#endif // SETUP_CRASH_DETAIL_HELPER
