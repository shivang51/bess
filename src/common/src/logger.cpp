#include "common/logger.h"
#include "spdlog/spdlog.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Bess::SimEngine {

    Logger &Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    void Logger::initLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<spdlog::sink_ptr> logSinks;

#ifdef DEBUG
        logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif
        logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(name + ".log", true));

        logSinks[0]->set_pattern("%^[%T] %n: %v%$");
        logSinks[1]->set_pattern("[%T] [%l] %n: %v");

        auto logger = std::make_shared<spdlog::logger>(name, begin(logSinks), end(logSinks));
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
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_loggers.find(name);
        if (it == m_loggers.end()) {
            initLogger(name);
        }
        return m_loggers.at(name);
    }
} // namespace Bess::SimEngine
