#include "bess_core/scene/scene_serializer.h"

#include "common/bess_uuid.h"
#include "bess_core/scene/scene.h"

namespace Bess {

    void SceneSerializer::serializeToPath(const std::string &path,
                                          int indent) const {
    }

    void SceneSerializer::deserializeFromPath(const std::string &path) {
    }

    void SceneSerializer::serialize(
        Json::Value &json, const std::shared_ptr<Canvas::Scene> &scene) const {
        JsonConvert::toJsonValue(scene->getState(), json["scene_state"]);
    }

    void SceneSerializer::serializeEntity(UUID uid, Json::Value &j) const {
    }

    void
    SceneSerializer::deserialize(Json::Value &json,
                                 const std::shared_ptr<Canvas::Scene> &scene) {
        deserialize(json, scene, {});
    }

    void
    SceneSerializer::deserialize(Json::Value &json,
                                 const std::shared_ptr<Canvas::Scene> &scene,
                                 const Canvas::SceneLoadCtx &ctx) {
        m_maxZ = 0;

        scene->clear();
        auto &state = scene->getState();
        JsonConvert::fromJsonValue(json["scene_state"], state, ctx);
    }

    void SceneSerializer::deserializeEntity(const Json::Value &json) {
    }

} // namespace Bess
