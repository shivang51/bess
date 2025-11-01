#pragma once

#include "plugin_handle.h"
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Bess::Plugins {
    PyGILState_STATE createPyThreadState();
    void releasePyThreadState(PyGILState_STATE state);
    void savePyThreadState();
    void restorePyThreadState();

    class PluginManager {
      public:
        static PluginManager &getInstance();
        static bool isIntialized;

        ~PluginManager();

        void init();
        void destroy();
        bool loadPlugin(const std::string &pluginPath);
        bool unloadPlugin(const std::string &pluginName);
        void unloadAllPlugins();
        bool loadPluginsFromDirectory(const std::string &pluginsDir = "plugins");

        std::vector<std::string> getLoadedPluginsNames() const;
        const std::unordered_map<std::string, std::shared_ptr<PluginHandle>> &getLoadedPlugins() const;
        bool isPluginLoaded(const std::string &pluginName) const;
        std::shared_ptr<PluginHandle> getPlugin(const std::string &pluginName) const;

      private:
        std::unordered_map<std::string, std::shared_ptr<PluginHandle>> m_plugins;
    };

} // namespace Bess::Plugins
