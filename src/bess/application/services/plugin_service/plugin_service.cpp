#include "plugin_service.h"
#include "common/logger.h"
#include "plugin_manager.h"

namespace Bess::Svc {
    void PluginService::onInit() {
        m_initialized = true;
        BESS_DEBUG("Plugin Service Intialized");
    }

    void PluginService::onPreInit() {
        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.loadPluginsFromDirectory("plugins");
    }

    void PluginService::onDestroy() {
        if (!m_initialized) {
            return;
        }
        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.destroy();
        m_initialized = false;
        BESS_DEBUG("Plugin Service Destroyed");
    }

    std::shared_ptr<Canvas::SimulationSceneComponent>
    PluginService::getSimSceneComp(
        const std::shared_ptr<SimEngine::Drivers::CompDef> &def) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            auto comp = plugin.second->getSimSceneComponent(def);
            if (comp) {
                return comp;
            }
        }

        return nullptr;
    }

    const std::unordered_map<std::string,
                             std::shared_ptr<Plugins::PluginHandle>> &
    PluginService::getPlugins() {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();
        return pluginMangaer.getLoadedPluginsRef();
    }

    bool PluginService::hasSimSceneComp(const std::string &defName) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            if (plugin.second->hasSimSceneComponent(defName)) {
                return true;
            }
        }

        return false;
    }

    bool PluginService::hasSceneComp(const std::string &typeName) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            if (plugin.second->hasSceneComp(typeName)) {
                return true;
            }
        }

        return false;
    }

    bool PluginService::canDerserialize(const std::string &typeName) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            if (plugin.second->canDerserialize(typeName)) {
                return true;
            }
        }

        return false;
    }

    std::shared_ptr<Canvas::SceneComponent>
    PluginService::derserialize(const std::string &typeName,
                                const Json::Value &json) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            if (plugin.second->canDerserialize(typeName)) {
                return plugin.second->derserialize(typeName, json);
            }
        }

        return nullptr;
    }
} // namespace Bess::Svc
