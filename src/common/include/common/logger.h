#pragma once

#include "spdlog/spdlog.h"
#include <string>

namespace Bess::SimEngine {
    class Logger {
      public:
        static Logger &getInstance();

        Logger() = default;
        void initLogger(const std::string &name);
        const std::shared_ptr<spdlog::logger> &getLogger(const std::string &name);

      private:
        std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> m_loggers;
        std::mutex m_mutex;
    };
} // namespace Bess::SimEngine

// idea from https://github.com/TheCherno/Hazel/blob/master/Hazel/src/Hazel/Core/Log.h
#ifdef DEBUG
    #define BESS_SE_TRACE(...) ::Bess::SimEngine::Logger::getInstance().getLogger("BessSimEngine")->trace(__VA_ARGS__)
    #define BESS_SE_INFO(...) ::Bess::SimEngine::Logger::getInstance().getLogger("BessSimEngine")->info(__VA_ARGS__)
    #define BESS_SE_WARN(...) ::Bess::SimEngine::Logger::getInstance().getLogger("BessSimEngine")->warn(__VA_ARGS__)
    #define BESS_SE_ERROR(...) ::Bess::SimEngine::Logger::getInstance().getLogger("BessSimEngine")->error(__VA_ARGS__)
    #define BESS_SE_CRITICAL(...) ::Bess::SimEngine::Logger::getInstance().getLogger("BessSimEngine")->critical(__VA_ARGS__)
#else
    #define BESS_SE_TRACE(...)
    #define BESS_SE_INFO(...)
    #define BESS_SE_WARN(...)
    #define BESS_SE_ERROR(...)
    #define BESS_SE_CRITICAL(...)
#endif
