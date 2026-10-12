#include "common/app_paths.h"

namespace Bess::AppPaths {
    namespace {
        std::filesystem::path logDirectory;
        std::filesystem::path configDirectory;
    } // namespace

    void setLogDirectory(const std::filesystem::path &dir) {
        logDirectory = dir;
    }

    const std::filesystem::path &getLogDirectory() {
        return logDirectory;
    }

    void setConfigDirectory(const std::filesystem::path &dir) {
        configDirectory = dir;
    }

    const std::filesystem::path &getConfigDirectory() {
        return configDirectory;
    }
} // namespace Bess::AppPaths
