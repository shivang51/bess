#include "spdlog/sinks/base_sink.h"
#include <mutex>
#include <string>
#include <vector>

namespace Bess {
    enum class LogLevel : uint8_t {
        trace = spdlog::level::trace,
        debug = spdlog::level::debug,
        info = spdlog::level::info,
        warn = spdlog::level::warn,
        error = spdlog::level::err,
        critical = spdlog::level::critical,
    };

    struct LogMessage {
        LogLevel level;
        std::string message;
    };

    template <typename Mutex>
    class UILogSink : public spdlog::sinks::base_sink<Mutex> {
      protected:
        void sink_it_(const spdlog::details::log_msg &msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

            std::lock_guard<std::mutex> lock(bufferMutex);
            logs.emplace_back((LogLevel)msg.level, fmt::to_string(formatted));

            if (logs.size() > 500)
                logs.erase(logs.begin());
        }

        void flush_() override {}

      public:
        std::vector<LogMessage> logs;
        std::mutex bufferMutex;
    };
} // namespace Bess

using SpdLogUISink = Bess::UILogSink<std::mutex>;
