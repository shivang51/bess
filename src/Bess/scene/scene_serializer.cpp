#include "scene/scene_serializer.h"

#include <algorithm>

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/scene.h"

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

        for (const auto &[uuid, comp] : state.getAllComponents()) {
            m_maxZ = std::max(comp->getTransform().position.z, m_maxZ);
        }

        scene->setZCoord(m_maxZ);
    }

    void SceneSerializer::deserializeEntity(const Json::Value &json) {
    }

} // namespace Bess
