#pragma once

#include "common/bess_api.h"
#include "common/file_watcher.h"
#include "plugin_handle.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pybind11 {
    class gil_scoped_release;
}

namespace Bess::Plugins {
    class BESS_API PluginManager {
      public:
        static PluginManager &getInstance();
        static bool isIntialized;

        PluginManager(const PluginManager &) = delete;
        PluginManager &operator=(const PluginManager &) = delete;
        PluginManager(PluginManager &&) = delete;
        PluginManager &operator=(PluginManager &&) = delete;

        ~PluginManager();

        void init();
        void destroy();
#ifdef DEBUG
        bool loadPlugin(const std::string &pluginPath, bool watchPlugin = true);
#else
        bool loadPlugin(const std::string &pluginPath,
                        bool watchPlugin = false);
#endif
        bool unloadPlugin(const std::string &pluginName);
        void unloadAllPlugins();
        bool
        loadPluginsFromDirectory(const std::string &pluginsDir = "plugins");

        bool reloadPlugin(const std::string &pluginName,
                          const std::filesystem::path &mainPath);

        std::vector<std::string> getLoadedPluginsNames() const;
        // Returning copy intentionally to avoid map corrouption while reloading
        std::unordered_map<std::string, std::shared_ptr<PluginHandle>>
        getLoadedPlugins() const;

        const std::unordered_map<std::string, std::shared_ptr<PluginHandle>> &
        getLoadedPluginsRef() const;

        bool isPluginLoaded(const std::string &pluginName) const;
        std::shared_ptr<PluginHandle>
        getPlugin(const std::string &pluginName) const;

      private:
        PluginManager() = default;

        bool loadPluginHandle(const std::filesystem::path &pluginPath,
                              bool reload,
                              std::string &pluginName,
                              std::shared_ptr<PluginHandle> &pluginHandle);

        std::unordered_map<std::string, std::shared_ptr<PluginHandle>>
            m_plugins;

#ifdef DEBUG
        std::vector<std::unique_ptr<Bess::Common::FileWatcher>>
            m_pluginFileWatchers;
#endif

        std::unique_ptr<pybind11::gil_scoped_release> m_gilRelease;
        mutable std::mutex m_pluginMutex;
    };

} // namespace Bess::Plugins
