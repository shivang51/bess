#pragma once

#include "common/bess_api.h"

#include "types.h"
#include <filesystem>
#include <string>
#include <thread>
#include <unordered_map>

namespace Bess::Common {

    struct BESS_API FileWatcherConfig {
        TimeMs checkInterval = TimeMs(1000.0);
        std::span<const std::string_view>
            extToWatch; // Will be priortized over ignore
        std::span<const std::string_view> extToIgnore;
    };

    class BESS_API FileWatcher {
      public:
        FileWatcher(const std::string &path,
                    const FileWatcherConfig &config = {});
        ~FileWatcher();

        typedef std::function<void(const std::string &filePath,
                                   const std::string &watchPath)>
            OnChangeCallback;

        void start(const OnChangeCallback &onChange);
        void stop();

        bool isRunning() const;

      private:
        std::string m_path;
        std::unordered_map<std::string, std::filesystem::file_time_type>
            m_files;
        FileWatcherConfig m_config;

        std::thread m_watchThread;
        std::atomic<bool> m_running{false};
    };

} // namespace Bess::Common
