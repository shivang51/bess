#include "common/logger.h"
#include "spdlog/spdlog.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Bess {

    Logger &Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    void Logger::initLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_initMutex);
        std::vector<spdlog::sink_ptr> logSinks;

#ifdef DEBUG
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("%^[%T] [%L] %n: %v%$");
        logSinks.push_back(std::move(consoleSink));
#endif

        if (!m_fileSink) {
            m_fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("bess.log", true);
            m_fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%l] [%n] %v");
        }

        logSinks.push_back(m_fileSink);

        if (m_uiSink) {
            m_uiSink->set_pattern("[%T] [%L] %v");
            logSinks.push_back(m_uiSink);
        }

        auto logger = std::make_shared<spdlog::logger>(name, logSinks.begin(), logSinks.end());
        spdlog::register_logger(logger);

#ifdef DEBUG
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);
#else
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
#endif

        m_loggers[name] = std::move(logger);
    }

    const std::shared_ptr<spdlog::logger> &Logger::getLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_getMutex);
        auto it = m_loggers.find(name);
        if (it == m_loggers.end()) {
            initLogger(name);
        }
        return m_loggers.at(name);
    }

    const std::shared_ptr<SpdLogUISink> &Logger::getUISink() {
        return Logger::getInstance().m_uiSink;
    }
} // namespace Bess
