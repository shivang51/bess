#include "plugin_manager.h"
#include "plugin_interface.h"
#include <filesystem>
#include <fstream>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <spdlog/spdlog.h>

namespace Bess {

    PluginManager::PluginManager() {
        pybind11::initialize_interpreter();
        spdlog::info("PluginManager initialized with Python interpreter");
    }

    PluginManager::~PluginManager() {
        unloadAllPlugins();
        pybind11::finalize_interpreter();
        spdlog::info("PluginManager destroyed");
    }

    bool PluginManager::loadPlugin(const std::string &pluginPath) {
        try {
            namespace py = pybind11;

            if (!std::filesystem::exists(pluginPath)) {
                spdlog::error("Plugin file not found: {}", pluginPath);
                return false;
            }

            std::filesystem::path path(pluginPath);
            std::string pluginName = path.stem().string();

            if (isPluginLoaded(pluginName)) {
                spdlog::warn("Plugin already loaded: {}", pluginName);
                return true;
            }

            py::module_ sys = py::module_::import("sys");
            py::list path_list = sys.attr("path");
            path_list.append(path.parent_path().string());

            py::module_ pluginModule = py::module_::import(pluginName.c_str());

            if (py::hasattr(pluginModule, "Plugin")) {
                py::object PluginClass = pluginModule.attr("Plugin");
                py::object pluginInstance = PluginClass();

                // Create a simple plugin interface wrapper
                // For now, just store the Python object
                // TODO: Create proper C++ wrapper class
                spdlog::info("Successfully loaded plugin: {}", pluginName);
                return true;
            } else {
                spdlog::error("Plugin {} does not have required 'Plugin' class", pluginName);
                return false;
            }

        } catch (const std::exception &e) {
            spdlog::error("Failed to load plugin {}: {}", pluginPath, e.what());
            return false;
        }
    }

    bool PluginManager::loadPluginsFromDirectory(const std::string &pluginsDir) {
        try {
            namespace fs = std::filesystem;

            if (!fs::exists(pluginsDir)) {
                spdlog::warn("Plugins directory does not exist: {}", pluginsDir);
                return false;
            }

            int loadedCount = 0;
            for (const auto &entry : fs::directory_iterator(pluginsDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".py") {
                    if (loadPlugin(entry.path().string())) {
                        loadedCount++;
                    }
                }
            }

            spdlog::info("Loaded {} plugins from directory: {}", loadedCount, pluginsDir);
            return loadedCount > 0;

        } catch (const std::exception &e) {
            spdlog::error("Failed to load plugins from directory {}: {}", pluginsDir, e.what());
            return false;
        }
    }

    bool PluginManager::unloadPlugin(const std::string &pluginName) {
        auto it = m_plugins.find(pluginName);
        if (it != m_plugins.end()) {
            it->second->shutdown();
            m_plugins.erase(it);
            spdlog::info("Plugin unloaded: {}", pluginName);
            return true;
        }
        spdlog::warn("Plugin not found for unloading: {}", pluginName);
        return false;
    }

    void PluginManager::unloadAllPlugins() {
        for (auto &[name, plugin] : m_plugins) {
            plugin->shutdown();
            spdlog::info("Plugin unloaded: {}", name);
        }
        m_plugins.clear();
    }

    std::vector<std::string> PluginManager::getLoadedPlugins() const {
        std::vector<std::string> pluginNames;
        for (const auto &[name, plugin] : m_plugins) {
            pluginNames.push_back(name);
        }
        return pluginNames;
    }

    bool PluginManager::isPluginLoaded(const std::string &pluginName) const {
        return m_plugins.find(pluginName) != m_plugins.end();
    }

    std::shared_ptr<PluginInterface> PluginManager::getPlugin(const std::string &pluginName) const {
        auto it = m_plugins.find(pluginName);
        if (it != m_plugins.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool PluginManager::executePluginFunction(const std::string &pluginName, const std::string &functionName) {
        auto plugin = getPlugin(pluginName);
        if (plugin) {
            return plugin->executeFunction(functionName);
        }
        spdlog::error("Plugin not found for function execution: {}", pluginName);
        return false;
    }

} // namespace Bess
