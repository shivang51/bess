#pragma once

#include <cstddef>
#include <filesystem>
#include <pybind11/pybind11.h>
#include <string>

namespace Bess::Plugins::HotReload {
    std::string makePluginModuleName(const std::filesystem::path &pluginPath);

    void putSysPathFirst(const std::filesystem::path &path);

    pybind11::dict collectLivePluginClasses(const std::filesystem::path &root);

    pybind11::dict removeCachedModules(const std::filesystem::path &root);

    void restoreCachedModules(const pybind11::dict &removedModules);

    size_t patchLivePluginClasses(const pybind11::dict &oldClasses,
                                  const std::filesystem::path &root);

    pybind11::module_
    importPluginModuleFromFile(const std::filesystem::path &pluginPath,
                               const std::string &moduleName);
} // namespace Bess::Plugins::HotReload
