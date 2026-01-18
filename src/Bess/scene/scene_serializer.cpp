#include "scene/scene_serializer.h"

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "plugin_manager.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include <algorithm>

using namespace Bess::Canvas::Components;

namespace Bess {

    void SceneSerializer::serializeToPath(const std::string &path, int indent) {
    }

    void SceneSerializer::deserializeFromPath(const std::string &path) {
    }

    void SceneSerializer::serialize(Json::Value &j) {
        const auto &state = Canvas::Scene::instance()->getState();
        JsonConvert::toJsonValue(state, j["scene_state"]);
    }

    void SceneSerializer::serializeEntity(UUID uid, Json::Value &j) {
    }

    void SceneSerializer::deserialize(const Json::Value &json) {
        m_maxZ = 0;

        auto scene = Canvas::Scene::instance();
        scene->clear();
        auto &state = scene->getState();
        JsonConvert::fromJsonValue(json["scene_state"], state);

        auto &simEngine = SimEngine::SimulationEngine::instance();

        const auto &pluginManager = Plugins::PluginManager::getInstance();

        for (const auto &[uuid, comp] : state.getAllComponents()) {
            m_maxZ = std::max(comp->getTransform().position.z, m_maxZ);
            if (comp->getType() == Canvas::SceneComponentType::simulation) {
                auto simComp = state.getComponentByUuid<Canvas::SimulationSceneComponent>(uuid);
                const auto &compDef = simEngine.getComponentDefinition(simComp->getSimEngineId());
                if (scene->hasPluginDrawHookForComponentHash(compDef->getBaseHash())) {
                    simComp->setDrawHook(scene->getPluginDrawHookForComponentHash(compDef->getBaseHash()));
                }
            }
        }
        scene->setZCoord(m_maxZ);
    }

    void SceneSerializer::deserializeEntity(const Json::Value &json) {
    }

} // namespace Bess
