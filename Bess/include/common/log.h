#pragma once

#include "logger.h"

#define BESS_LOGGER_NAME "Bess"
#define BESS_TRACE(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->trace(__VA_ARGS__)
#define BESS_INFO(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->info(__VA_ARGS__)
#define BESS_WARN(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->warn(__VA_ARGS__)
#define BESS_ERROR(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->error(__VA_ARGS__)
#define BESS_CRITICAL(...) ::Bess::SimEngine::Logger::getInstance().getLogger(BESS_LOGGER_NAME)->critical(__VA_ARGS__)
