#include "plugin_manager.h"
#include "common/file_watcher.h"
#include "common/logger.h"
#include "plugin_handle.h"
#include <filesystem>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pystate.h>

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
        pybind11::initialize_interpreter(false);

        pybind11::module_ sys = pybind11::module_::import("sys");
        pybind11::list path_list = sys.attr("path");

#ifdef DEBUG
        path_list.append("src/bessplug/py");
#else
        path_list.append("bindings/bessplug");
#endif
        pybind11::gil_scoped_release gil;
        BESS_INFO("PluginManager initialized with Python interpreter");
        isIntialized = true;
    }

    void PluginManager::destroy() {
        if (!isIntialized)
            return;
        {
            unloadAllPlugins();
        }
        pybind11::finalize_interpreter();
        BESS_INFO("PluginManager destroyed");

        isIntialized = false;
    }

    PluginManager::~PluginManager() {
        destroy();
    }

    bool PluginManager::loadPlugin(const std::string &pluginPath,
                                   bool watchPlugin) {
        try {
            namespace py = pybind11;

            if (!std::filesystem::exists(pluginPath)) {
                BESS_ERROR("Plugin file not found: {}", pluginPath);
                return false;
            }

            const std::filesystem::path path(pluginPath);
            const std::string mainFile = path.stem().string();
            std::string pluginName;

            {
                BESS_INFO("Loading plugin from file: {}", pluginPath);
                pybind11::gil_scoped_acquire gil;
                py::module_ sys = py::module_::import("sys");
                py::list path_list = sys.attr("path");
                path_list.append(path.parent_path().string());

                bool hasMainModule =
                    py::module::import("sys").attr("modules").contains(
                        mainFile.c_str());

                if (hasMainModule) {
                    BESS_WARN("Plugin module already in sys.modules: {}. "
                              "This may cause unexpected behavior.",
                              mainFile);

                    return false;
                }

                auto pluginModule = py::module::import(mainFile.c_str());

                if (py::hasattr(pluginModule, "plug_hwd")) {
                    BESS_ERROR(
                        "Plugin {} does not have required 'plug_hwd' variable",
                        mainFile);
                    return false;
                }

                py::object pluginHwd = pluginModule.attr("plugin_hwd");
                pluginName = pluginHwd.attr("name").cast<std::string>();

                if (isPluginLoaded(pluginName)) {
                    BESS_WARN("Plugin already loaded: {}", mainFile);
                    return true;
                }

                m_plugins[pluginName] =
                    std::make_shared<PluginHandle>(pluginHwd);
                BESS_INFO("Successfully loaded plugin: {} from {}",
                          pluginName,
                          path.parent_path().string());

                if (!watchPlugin) {
                    return true;
                }
            }

            static constexpr std::array<const std::string_view, 1> extsToWatch =
                {".py"};

            Common::FileWatcherConfig watcherConfig;
            watcherConfig.extToWatch = extsToWatch;

            auto watcher = std::make_unique<Common::FileWatcher>(
                path.parent_path().string(), watcherConfig);

            watcher->start([this, pluginName, path](
                               const std::string &changedFile,
                               const std::string &watchPath) {
                BESS_WARN("Detected change in plugin file: {}", changedFile);

                if (!reloadPlugin(pluginName, path)) {
                    BESS_ERROR("Failed to reload plugin: {}", pluginName);
                }
            });

            m_pluginFileWatchers.emplace_back(std::move(watcher));

            return true;
        } catch (const std::exception &e) {
            BESS_ERROR("Failed to load plugin {}: {}", pluginPath, e.what());
            return false;
        }
    }

    bool
    PluginManager::loadPluginsFromDirectory(const std::string &pluginsDir) {
        try {
            namespace fs = std::filesystem;

            if (!fs::exists(pluginsDir)) {
                BESS_WARN("Plugins directory does not exist: {}", pluginsDir);
                return false;
            }

            int loadedCount = 0;
            for (const auto &entry : fs::directory_iterator(pluginsDir)) {
                if (!entry.is_directory())
                    continue;
                const auto file = entry.path() / "main.py";
                if (std::filesystem::exists(file) &&
                    loadPlugin(file.string())) {
                    loadedCount++;
                }
            }

            BESS_INFO("Loaded {} plugins from directory: {}",
                      loadedCount,
                      pluginsDir);
            return loadedCount > 0;

        } catch (const std::exception &e) {
            BESS_ERROR("Failed to load plugins from directory {}: {}",
                       pluginsDir,
                       e.what());
            return false;
        }
    }

    bool PluginManager::reloadPlugin(const std::string &pluginName,
                                     const std::filesystem::path &mainPath) {
        pybind11::gil_scoped_acquire gil;
        std::scoped_lock<std::mutex> lock(m_pluginMutex);
        BESS_INFO("Reloading plugin: {}", pluginName);
        if (!unloadPlugin(pluginName)) {
            BESS_ERROR("Failed to unload plugin: {}", pluginName);
            return false;
        }

        if (!loadPlugin(mainPath, false)) {
            BESS_ERROR("Failed to load plugin {} from {}",
                       pluginName,
                       mainPath.string());
            return false;
        }

        BESS_INFO("Successfully reloaded plugin: {}", pluginName);
        return true;
    }

    bool PluginManager::unloadPlugin(const std::string &pluginName) {
        pybind11::gil_scoped_acquire gil;
        auto it = m_plugins.find(pluginName);
        if (it == m_plugins.end()) {
            BESS_WARN("Plugin not found for unloading: {}", pluginName);
            return false;
        }

        BESS_WARN("Unloading plugin: {}", pluginName);

        try {
            namespace py = pybind11;
            auto sys = py::module_::import("sys");
            auto modules = sys.attr("modules");
            modules.attr("pop")("main", py::none());

            BESS_DEBUG("Removed plugin module from sys.modules: {}", "main");
        } catch (const std::exception &e) {
            BESS_ERROR("Failed to clear python module cache for {}: {}",
                       pybind11::str(it->second->getPluginObject())
                           .cast<std::string>(),
                       e.what());
        }

        m_plugins.erase(it);
        BESS_INFO("Successfully unloaded plugin: {}", pluginName);
        return true;
    }

    void PluginManager::unloadAllPlugins() {
        pybind11::gil_scoped_acquire gil;
        m_plugins.clear();
    }

    std::vector<std::string> PluginManager::getLoadedPluginsNames() const {
        std::vector<std::string> pluginNames;
        pluginNames.reserve(m_plugins.size());
        for (const auto &[name, plugin] : m_plugins) {
            pluginNames.push_back(name);
        }
        return pluginNames;
    }

    std::unordered_map<std::string, std::shared_ptr<PluginHandle>>
    PluginManager::getLoadedPlugins() const {
        std::scoped_lock<std::mutex> lock(m_pluginMutex);
        return m_plugins;
    }

    bool PluginManager::isPluginLoaded(const std::string &pluginName) const {
        return m_plugins.contains(pluginName);
    }

    std::shared_ptr<PluginHandle>
    PluginManager::getPlugin(const std::string &pluginName) const {
        std::scoped_lock<std::mutex> lock(m_pluginMutex);
        auto it = m_plugins.find(pluginName);
        if (it != m_plugins.end()) {
            return it->second;
        }
        return nullptr;
    }
} // namespace Bess::Plugins
