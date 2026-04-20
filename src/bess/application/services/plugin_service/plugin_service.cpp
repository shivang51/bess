#include "plugin_service.h"
#include "common/logger.h"
#include "plugin_manager.h"

namespace Bess::Svc {
    void PluginService::init() {
        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.loadPluginsFromDirectory("plugins");

        m_initialized = true;
        BESS_DEBUG("Plugin Service Intialized");
    }

    void PluginService::destroy() {
        if (!m_initialized) {
            return;
        }
        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.destroy();
        m_initialized = false;
        BESS_DEBUG("Plugin Service Destroyed");
    }

    PluginService &PluginService::getInstance() {
        static PluginService instance;
        return instance;
    }

    std::shared_ptr<Canvas::SimulationSceneComponent> PluginService::getSimComp(
        const std::shared_ptr<SimEngine::ComponentDefinition> &def) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            auto comp = plugin.second->getSceneComponent(def);
            if (comp) {
                return comp;
            }
        }

        return nullptr;
    }

    bool PluginService::hasSceneCompWithName(const std::string &name) const {
        const auto &pluginMangaer = Plugins::PluginManager::getInstance();

        for (const auto &plugin : pluginMangaer.getLoadedPlugins()) {
            if (plugin.second->hasSceneCompWithName(name)) {
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

    std::shared_ptr<Canvas::SceneComponent> PluginService::derserialize(
        const std::string &typeName,
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
