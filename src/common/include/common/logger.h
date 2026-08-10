#pragma once

#include "common/bess_api.h"

#include "glm.hpp"
#include "spdlog/sinks/basic_file_sink.h"
#include "ui_log_sink.h"

#include "spdlog/logger.h"
#include <memory>
#include <string>

namespace Bess {

    class BESS_API Logger {
      public:
        static Logger &getInstance();
        static const std::shared_ptr<SpdLogUISink> &getUISink();

        const std::shared_ptr<spdlog::logger> &
        getLogger(const std::string &name);

      private:
        Logger() = default;
        void initLogger(const std::string &name);

      private:
        std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>
            m_loggers;
        std::mutex m_initMutex;
        std::mutex m_getMutex;
        std::shared_ptr<SpdLogUISink> m_uiSink =
            std::make_shared<SpdLogUISink>();
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> m_fileSink = nullptr;
    };
} // namespace Bess

#ifndef LOGGER_NAME
    #define LOGGER_NAME "Default"
#endif

template <glm::length_t L, typename T, glm::qualifier Q>
struct std::formatter<glm::vec<L, T, Q>> : std::formatter<std::string> {
    auto format(const glm::vec<L, T, Q> &vector, std::format_context &ctx) const
        -> decltype(ctx.out()) {
        auto out = std::format_to(ctx.out(), "[");
        for (glm::length_t i = 0; i < L; ++i) {
            out =
                std::format_to(out, "{}{}", vector[i], (i + 1 < L) ? ", " : "");
        }
        return std::format_to(out, "]");
    }
};

#define LOGGER(name) ::Bess::Logger::getInstance().getLogger(name)
#define BESS_INFO(...) LOGGER(LOGGER_NAME)->info(std::format(__VA_ARGS__))
#define BESS_WARN(...) LOGGER(LOGGER_NAME)->warn(std::format(__VA_ARGS__))
#define BESS_ERROR(...) LOGGER(LOGGER_NAME)->error(std::format(__VA_ARGS__))
#define BESS_CRITICAL(...)                                                     \
    LOGGER(LOGGER_NAME)->critical(std::format(_VA_ARGS__))

#ifdef DEBUG
    #define BESS_TRACE(...)                                                    \
        LOGGER(LOGGER_NAME)                                                    \
            ->trace(                                                           \
                "[{}:{}] {}", __FILE__, __LINE__, std::format(__VA_ARGS__))
    #define BESS_DEBUG_F(...)                                                  \
        LOGGER(LOGGER_NAME)                                                    \
            ->debug(                                                           \
                "[{}:{}] {}", __FILE__, __LINE__, std::format(__VA_ARGS__))
    #define BESS_DEBUG(...) LOGGER(LOGGER_NAME)->debug(std::format(__VA_ARGS__))
#else
    #define BESS_TRACE(...)
    #define BESS_DEBUG(...)
    #define BESS_DEBUG_F(...)
#endif
