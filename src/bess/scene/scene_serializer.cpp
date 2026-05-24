#include "scene/scene_serializer.h"

#include "common/bess_uuid.h"
#include "scene/scene.h"

namespace Bess {

    void SceneSerializer::serializeToPath(const std::string &path,
                                          int indent) const {}

    void SceneSerializer::deserializeFromPath(const std::string &path) {}

    void SceneSerializer::serialize(
        Json::Value &json, const std::shared_ptr<Canvas::Scene> &scene) const {
        JsonConvert::toJsonValue(scene->getState(), json["scene_state"]);
    }

    void SceneSerializer::serializeEntity(UUID uid, Json::Value &j) const {}

    void
    SceneSerializer::deserialize(Json::Value &json,
                                 const std::shared_ptr<Canvas::Scene> &scene) {
        m_maxZ = 0;

        scene->clear();
        auto &state = scene->getState();
        JsonConvert::fromJsonValue(json["scene_state"], state);
    }

    void SceneSerializer::deserializeEntity(const Json::Value &json) {}

} // namespace Bess
