#include "plugin_manager.h"
#include "plugin_handle.h"
#include <filesystem>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <spdlog/spdlog.h>

namespace Bess::Plugins {
    PluginManager &PluginManager::getInstance() {
        static PluginManager instance;
        if (!isIntialized)
            instance.init();
        return instance;
    }

    bool PluginManager::isIntialized = false;

    void PluginManager::init() {
        if (isIntialized)
            return;
        pybind11::initialize_interpreter();
        spdlog::info("PluginManager initialized with Python interpreter");
        isIntialized = true;
    }

    void PluginManager::destroy() {
        if (!isIntialized)
            return;
        unloadAllPlugins();
        pybind11::finalize_interpreter();
        spdlog::info("PluginManager destroyed");

        isIntialized = false;
    }

    PluginManager::~PluginManager() {
        destroy();
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

            py::gil_scoped_acquire gil;
            py::module_ sys = py::module_::import("sys");

            py::list path_list = sys.attr("path");
            path_list.append("bessplug");
            path_list.append(path.parent_path().string());

            py::module_ pluginModule = py::module::import(pluginName.c_str());

            if (py::hasattr(pluginModule, "plugin_hwd")) {
                py::object pluginHwd = pluginModule.attr("plugin_hwd");
                auto name = pluginHwd.attr("name").cast<std::string>();

                m_plugins[name] = std::make_shared<PluginHandle>(pluginHwd);

                spdlog::info("Successfully loaded plugin: {} from {}", name, path.parent_path().string());
                return true;
            } else {
                spdlog::error("Plugin {} does not have required 'plug_hwd' variable", pluginName);
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
                if (!entry.is_directory())
                    continue;
                for (const auto &file : fs::directory_iterator(entry)) {
                    if (file.is_regular_file() && file.path().extension() == ".py") {
                        if (loadPlugin(file.path().string())) {
                            loadedCount++;
                        }
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
        spdlog::warn("Plugin not found for unloading: {}", pluginName);
        return false;
    }

    void PluginManager::unloadAllPlugins() {
        m_plugins.clear();
    }

    std::vector<std::string> PluginManager::getLoadedPlugins() const {
        std::vector<std::string> pluginNames;
        pluginNames.reserve(m_plugins.size());
        for (const auto &[name, plugin] : m_plugins) {
            pluginNames.push_back(name);
        }
        return pluginNames;
    }

    bool PluginManager::isPluginLoaded(const std::string &pluginName) const {
        return m_plugins.contains(pluginName);
    }

    std::shared_ptr<PluginHandle> PluginManager::getPlugin(const std::string &pluginName) const {
        auto it = m_plugins.find(pluginName);
        if (it != m_plugins.end()) {
            return it->second;
        }
        return nullptr;
    }
} // namespace Bess::Plugins
