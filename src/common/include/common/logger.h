#pragma once

#include "spdlog/logger.h"
#include <string>

namespace Bess::SimEngine {
    class Logger {
      public:
        static Logger &getInstance();
        const std::shared_ptr<spdlog::logger> &getLogger(const std::string &name);

      private:
        Logger() = default;
        void initLogger(const std::string &name);

      private:
        std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
        std::mutex m_mutex;
    };
} // namespace Bess::SimEngine

#ifndef LOGGER_NAME
    #define LOGGER_NAME "Default"
#endif

// idea from https://github.com/TheCherno/Hazel/blob/master/Hazel/src/Hazel/Core/Log.h
#define LOGGER(name) ::Bess::SimEngine::Logger::getInstance().getLogger(name)
#ifdef DEBUG
    #define BESS_TRACE(...) LOGGER(LOGGER_NAME)->trace(__VA_ARGS__)
    #define BESS_INFO(...) LOGGER(LOGGER_NAME)->info(__VA_ARGS__)
    #define BESS_WARN(...) LOGGER(LOGGER_NAME)->warn(__VA_ARGS__)
    #define BESS_ERROR(...) LOGGER(LOGGER_NAME)->error(__VA_ARGS__)
    #define BESS_CRITICAL(...) LOGGER(LOGGER_NAME)->critical(__VA_ARGS__)
    #define BESS_DEBUG(...) LOGGER(LOGGER_NAME)->debug(__VA_ARGS__)
#else
    #define BESS_TRACE(...)
    #define BESS_INFO(...)
    #define BESS_WARN(...)
    #define BESS_ERROR(...)
    #define BESS_CRITICAL(...)
    #define BESS_DEBUG(...)
#endif
