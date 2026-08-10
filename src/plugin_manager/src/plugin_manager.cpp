#include "plugin_manager.h"
#include "common/file_watcher.h"
#include "common/logger.h"
#include "plugin_handle.h"
#include "plugin_hot_reload.h"
#include <filesystem>
#include <mutex>
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

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

        {
            pybind11::module_ sys = pybind11::module_::import("sys");
            pybind11::list path_list = sys.attr("path");

#ifdef DEBUG
            path_list.append("src/bessplug/py");
#else
            path_list.append("bindings/bessplug");
#endif
        }

        m_gilRelease = std::make_unique<pybind11::gil_scoped_release>();
        BESS_INFO("PluginManager initialized with Python interpreter");
        isIntialized = true;
    }

    void PluginManager::destroy() {
        if (!isIntialized)
            return;

#ifdef DEBUG
        for (auto &watcher : m_pluginFileWatchers) {
            watcher->stop();
        }
        m_pluginFileWatchers.clear();
#endif

        unloadAllPlugins();
        m_gilRelease.reset();
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
            if (!std::filesystem::exists(pluginPath)) {
                BESS_ERROR("Plugin file not found: {}", pluginPath);
                return false;
            }

            const std::filesystem::path path(pluginPath);
            std::string pluginName;
            std::shared_ptr<PluginHandle> pluginHandle;
            if (!loadPluginHandle(path, false, pluginName, pluginHandle)) {
                return false;
            }

            {
                std::scoped_lock<std::mutex> lock(m_pluginMutex);
                if (m_plugins.contains(pluginName)) {
                    BESS_WARN("Plugin already loaded: {}", pluginName);
                    return true;
                }
                m_plugins[pluginName] = std::move(pluginHandle);
            }

            BESS_INFO("Successfully loaded plugin: {} from {}",
                      pluginName,
                      path.parent_path().string());

#ifdef DEBUG
            if (!watchPlugin) {
                return true;
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
#endif

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
        BESS_INFO("Reloading plugin: {}", pluginName);

        std::string reloadedPluginName;
        std::shared_ptr<PluginHandle> reloadedPlugin;
        if (!loadPluginHandle(
                mainPath, true, reloadedPluginName, reloadedPlugin)) {
            BESS_ERROR("Failed to load plugin {} from {}",
                       pluginName,
                       mainPath.string());
            return false;
        }

        std::shared_ptr<PluginHandle> oldPlugin;
        {
            std::scoped_lock<std::mutex> lock(m_pluginMutex);
            auto it = m_plugins.find(pluginName);
            if (it == m_plugins.end()) {
                BESS_WARN("Plugin not found for reloading: {}", pluginName);
                return false;
            }

            oldPlugin = std::move(it->second);
            m_plugins.erase(it);
            m_plugins[reloadedPluginName] = std::move(reloadedPlugin);
        }

        if (reloadedPluginName != pluginName) {
            BESS_WARN("Plugin name changed during reload: {} -> {}",
                      pluginName,
                      reloadedPluginName);
        }

        {
            pybind11::gil_scoped_acquire gil;
            oldPlugin.reset();
        }

        BESS_INFO("Successfully reloaded plugin: {}", reloadedPluginName);
        return true;
    }

    bool PluginManager::unloadPlugin(const std::string &pluginName) {
        std::shared_ptr<PluginHandle> plugin;
        {
            std::scoped_lock<std::mutex> lock(m_pluginMutex);
            auto it = m_plugins.find(pluginName);
            if (it == m_plugins.end()) {
                BESS_WARN("Plugin not found for unloading: {}", pluginName);
                return false;
            }
            plugin = std::move(it->second);
            m_plugins.erase(it);
        }

        BESS_WARN("Unloading plugin: {}", pluginName);
        {
            pybind11::gil_scoped_acquire gil;
            plugin.reset();
        }
        BESS_INFO("Successfully unloaded plugin: {}", pluginName);
        return true;
    }

    void PluginManager::unloadAllPlugins() {
        std::unordered_map<std::string, std::shared_ptr<PluginHandle>> plugins;
        {
            std::scoped_lock<std::mutex> lock(m_pluginMutex);
            plugins.swap(m_plugins);
        }

        pybind11::gil_scoped_acquire gil;
        plugins.clear();
    }

    std::vector<std::string> PluginManager::getLoadedPluginsNames() const {
        std::scoped_lock<std::mutex> lock(m_pluginMutex);
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
        std::scoped_lock<std::mutex> lock(m_pluginMutex);
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

    bool PluginManager::loadPluginHandle(
        const std::filesystem::path &pluginPath,
        bool reload,
        std::string &pluginName,
        std::shared_ptr<PluginHandle> &pluginHandle) {
        try {
            namespace py = pybind11;

            py::gil_scoped_acquire gil;
            const auto absolutePluginPath =
                std::filesystem::absolute(pluginPath).lexically_normal();
            const auto pluginRoot = absolutePluginPath.parent_path();
            const auto moduleName =
                HotReload::makePluginModuleName(absolutePluginPath);

            HotReload::putSysPathFirst(pluginRoot);
            py::dict liveClasses;
            py::dict removedModules;
            if (reload) {
                liveClasses = HotReload::collectLivePluginClasses(pluginRoot);
                removedModules = HotReload::removeCachedModules(pluginRoot);
            }

            BESS_INFO("{} plugin from file: {}",
                      reload ? "Reloading" : "Loading",
                      absolutePluginPath.string());

            py::module_ pluginModule;
            try {
                pluginModule = HotReload::importPluginModuleFromFile(
                    absolutePluginPath, moduleName);
            } catch (...) {
                if (reload) {
                    HotReload::restoreCachedModules(removedModules);
                }
                throw;
            }

            if (reload) {
                const auto patchedClassCount =
                    HotReload::patchLivePluginClasses(liveClasses, pluginRoot);
                BESS_DEBUG("Patched {} live plugin classes after reload",
                           patchedClassCount);
            }

            if (!py::hasattr(pluginModule, "plugin_hwd")) {
                BESS_ERROR(
                    "Plugin {} does not have required 'plugin_hwd' variable",
                    absolutePluginPath.string());
                return false;
            }

            py::object pluginHwd = pluginModule.attr("plugin_hwd");
            pluginName = pluginHwd.attr("name").cast<std::string>();
            pluginHandle = std::make_shared<PluginHandle>(pluginHwd);
            return true;
        } catch (const std::exception &e) {
            BESS_ERROR(
                "Failed to load plugin {}: {}", pluginPath.string(), e.what());
            return false;
        }
    }
} // namespace Bess::Plugins
