#include "log.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Bess::Vulkan {
    Logger &Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    void Logger::initLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<spdlog::sink_ptr> logSinks;
        logSinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        logSinks.emplace_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(name + ".log", true));

        logSinks[0]->set_pattern("%^[%T] %n: %v%$");
        logSinks[1]->set_pattern("[%T] [%l] %n: %v");

        auto logger = std::make_shared<spdlog::logger>(name, begin(logSinks), end(logSinks));
        spdlog::register_logger(logger);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);

        m_loggers[name] = std::move(logger);
    }

    const std::shared_ptr<spdlog::logger> &Logger::getLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_loggers[name];
    }
} // namespace Bess::Vulkan
