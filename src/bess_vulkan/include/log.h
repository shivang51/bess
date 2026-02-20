#pragma once

#include "spdlog/spdlog.h"
#include <string>

namespace Bess::Vulkan {
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
} // namespace Bess::Vulkan

#ifdef DEBUG
    #define BESS_VK_TRACE(...) ::Bess::Vulkan::Logger::getInstance().getLogger("BessVulkan")->trace(__VA_ARGS__)
    #define BESS_VK_INFO(...) ::Bess::Vulkan::Logger::getInstance().getLogger("BessVulkan")->info(__VA_ARGS__)
    #define BESS_VK_WARN(...) ::Bess::Vulkan::Logger::getInstance().getLogger("BessVulkan")->warn(__VA_ARGS__)
    #define BESS_VK_ERROR(...) ::Bess::Vulkan::Logger::getInstance().getLogger("BessVulkan")->error(__VA_ARGS__)
    #define BESS_VK_CRITICAL(...) ::Bess::Vulkan::Logger::getInstance().getLogger("BessVulkan")->critical(__VA_ARGS__)
#else
    #define BESS_VK_TRACE(...)
    #define BESS_VK_INFO(...)
    #define BESS_VK_WARN(...)
    #define BESS_VK_ERROR(...)
    #define BESS_VK_CRITICAL(...)
#endif
