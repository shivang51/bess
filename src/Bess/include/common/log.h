#pragma once

#include "logger.h"
#include <cassert>
#include <cstdlib>
#include <iostream>

// The do { … } while(false) idiom around a multi-statement macro is there to make it behave like a single statement in all contexts.
#define BESS_ASSERT(expr, msg)                           \
    do {                                                 \
        if (!(expr)) {                                   \
            std::cerr                                    \
                << "Assertion failed: (" << #expr << ")" \
                << "  File: " << __FILE__ << ":"         \
                << "  Line: " << __LINE__ << "\n"        \
                << "  Message: " << msg << std::endl;    \
            std::abort();                                \
        }                                                \
    } while (false)

#define BESS_LOGGER_NAME "Bess"
#define BESS_TRACE(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->trace(__VA_ARGS__)
#define BESS_INFO(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->info(__VA_ARGS__)
#define BESS_WARN(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->warn(__VA_ARGS__)
#define BESS_ERROR(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->error(__VA_ARGS__)
#define BESS_CRITICAL(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->critical(__VA_ARGS__)
