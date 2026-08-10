#include "common/file_watcher.h"
#include "logger.h"
#include <algorithm>
#include <filesystem>

namespace Bess::Common {
    FileWatcher::FileWatcher(const std::string &path,
                             const FileWatcherConfig &config)
        : m_path(path),
          m_config(config) {

        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();

                if (!m_config.extToWatch.empty()) {
                    if (std::ranges::find(m_config.extToWatch, ext) !=
                        m_config.extToWatch.end()) {
                        m_files[entry.path().string()] =
                            std::filesystem::last_write_time(entry);
                    }
                    continue;
                }

                if (!m_config.extToIgnore.empty()) {
                    if (std::ranges::find(m_config.extToIgnore, ext) !=
                        m_config.extToIgnore.end()) {
                        continue;
                    }
                }

                m_files[entry.path().string()] =
                    std::filesystem::last_write_time(entry);
            }
        }

        BESS_DEBUG("Initialized FileWatcher for path: {} | Found {} files.",
                   path,
                   m_files.size());
    }

    FileWatcher::~FileWatcher() {
        stop();
    }

    void FileWatcher::start(const OnChangeCallback &onChange) {
        if (m_running) {
            return;
        }

        const auto runLoop = [this, onChange]() {
            BESS_DEBUG("Starting FileWatcher for path: {}", m_path);
            while (m_running) {
                std::this_thread::sleep_for(m_config.checkInterval);

                for (const auto &[filePath, lastWriteTime] : m_files) {
                    auto currentWriteTime =
                        std::filesystem::last_write_time(filePath);
                    if (currentWriteTime != lastWriteTime) {
                        m_files[filePath] = currentWriteTime;
                        BESS_DEBUG("File changed: {}", filePath);
                        onChange(filePath, m_path);
                    }
                }
            }
            BESS_DEBUG("Stopped FileWatcher for path: {}", m_path);
        };

        m_running = true;
        m_watchThread = std::thread(runLoop);
    }

    void FileWatcher::stop() {
        m_running = false;
        if (m_watchThread.joinable() &&
            m_watchThread.get_id() != std::this_thread::get_id()) {
            m_watchThread.join();
        }
    }

    bool FileWatcher::isRunning() const {
        return m_running;
    }

} // namespace Bess::Common
