#pragma once

#include "common/bess_api.h"
#include <filesystem>

namespace Bess::AppPaths {
    // Directories Bess writes its own files to (bess.log, bess.ini). They are
    // empty by default, which means the working directory. main() points them
    // at per-user locations where the working directory is read-only, such as
    // inside the macOS app bundle.
    BESS_API void setLogDirectory(const std::filesystem::path &dir);
    BESS_API const std::filesystem::path &getLogDirectory();

    BESS_API void setConfigDirectory(const std::filesystem::path &dir);
    BESS_API const std::filesystem::path &getConfigDirectory();
} // namespace Bess::AppPaths
