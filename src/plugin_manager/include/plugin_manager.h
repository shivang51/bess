#pragma once

#include "common/bess_api.h"
#include "common/file_watcher.h"
#include "plugin_handle.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Plugins {
    class BESS_API PluginManager {
      public:
        static PluginManager &getInstance();
        static bool isIntialized;

        ~PluginManager();

        void init();
        void destroy();
        bool loadPlugin(const std::string &pluginPath, bool watchPlugin = true);
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
        bool isPluginLoaded(const std::string &pluginName) const;
        std::shared_ptr<PluginHandle>
        getPlugin(const std::string &pluginName) const;

      private:
        std::unordered_map<std::string, std::shared_ptr<PluginHandle>>
            m_plugins;
        std::vector<std::unique_ptr<Bess::Common::FileWatcher>>
            m_pluginFileWatchers;

        mutable std::mutex m_pluginMutex;
    };

} // namespace Bess::Plugins
