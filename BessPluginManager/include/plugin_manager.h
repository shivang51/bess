#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess {

    class PluginInterface;

    class PluginManager {
      public:
        PluginManager();
        ~PluginManager();

        bool loadPlugin(const std::string &pluginPath);
        bool unloadPlugin(const std::string &pluginName);
        void unloadAllPlugins();
        bool loadPluginsFromDirectory(const std::string &pluginsDir = "plugins");

        std::vector<std::string> getLoadedPlugins() const;
        bool isPluginLoaded(const std::string &pluginName) const;
        std::shared_ptr<PluginInterface> getPlugin(const std::string &pluginName) const;

        bool executePluginFunction(const std::string &pluginName, const std::string &functionName);

      private:
        std::unordered_map<std::string, std::shared_ptr<PluginInterface>> m_plugins;
    };

} // namespace Bess
