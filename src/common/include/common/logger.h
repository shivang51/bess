#pragma once

#include "ui_log_sink.h"

#include "spdlog/logger.h"
#include <memory>
#include <string>

namespace Bess {

    class Logger {
      public:
        static Logger &getInstance();
        static const std::shared_ptr<SpdLogUISink> &getUISink();

        const std::shared_ptr<spdlog::logger> &getLogger(const std::string &name);

      private:
        Logger() = default;
        void initLogger(const std::string &name);

      private:
        std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
        std::mutex m_initMutex;
        std::mutex m_getMutex;
        std::shared_ptr<SpdLogUISink> m_uiSink = std::make_shared<SpdLogUISink>();
    };
} // namespace Bess

#ifndef LOGGER_NAME
    #define LOGGER_NAME "Default"
#endif

// idea from https://github.com/TheCherno/Hazel/blob/master/Hazel/src/Hazel/Core/Log.h
#define LOGGER(name) ::Bess::Logger::getInstance().getLogger(name)
#define BESS_INFO(...) LOGGER(LOGGER_NAME)->info(__VA_ARGS__)
#define BESS_WARN(...) LOGGER(LOGGER_NAME)->warn(__VA_ARGS__)
#define BESS_ERROR(...) LOGGER(LOGGER_NAME)->error(__VA_ARGS__)
#define BESS_CRITICAL(...) LOGGER(LOGGER_NAME)->critical(__VA_ARGS__)

#ifdef DEBUG
    #define BESS_TRACE(...) LOGGER(LOGGER_NAME)->trace("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
    #define BESS_DEBUG(...) LOGGER(LOGGER_NAME)->debug("[{}:{}] {}", __FILE__, __LINE__, fmt::format(__VA_ARGS__))
#else
    #define BESS_TRACE(...)
    #define BESS_DEBUG(...)
#endif
