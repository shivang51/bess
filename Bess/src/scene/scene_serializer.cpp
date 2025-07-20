#include "scene/scene_serializer.h"

#include "scene/scene.h"

using namespace Bess::Canvas::Components;

namespace Bess {

    void SceneSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(Canvas::Scene::instance().getEnttRegistry(), path, indent);
    }

    void SceneSerializer::deserializeFromPath(const std::string &path) {
        auto &reg = Canvas::Scene::instance().getEnttRegistry();
        reg.clear();
        EnttRegistrySerializer::deserializeFromPath(reg, path);
    }

    void SceneSerializer::serialize(nlohmann::json& j) {
        EnttRegistrySerializer::serialize(Canvas::Scene::instance().getEnttRegistry(), j);
    }

    void SceneSerializer::deserialize(const nlohmann::json &json) {
        m_maxZ = 0;

        auto &scene = Canvas::Scene::instance();
        scene.clear();

        auto &reg = scene.getEnttRegistry();
        EnttRegistrySerializer::deserialize(reg, json);

        scene.setZCoord(m_maxZ);
    }

    void SceneSerializer::registerAll() {
        registerComponent<IdComponent>("IdComponent");
        registerComponent<TransformComponent>("TransformComponent");
        registerComponent<SpriteComponent>("SpriteComponent");
        registerComponent<TagComponent>("TagComponent");
        registerComponent<SlotComponent>("SlotComponent");
        registerComponent<SimulationComponent>("SimulationComponent");
        registerComponent<ConnectionSegmentComponent>("ConnectionSegmentComponent");
        registerComponent<ConnectionComponent>("ConnectionComponent");
        registerComponent<SimulationOutputComponent>("SimulationOutputComponent");
        registerComponent<SimulationInputComponent>("SimulationInputComponent");
        registerComponent<TextNodeComponent>("TextNodeComponent");
    }
        // // TransformComponent
        // if (j.contains("TransformComponent")) {
        //     auto transformComp = j.at("TransformComponent").get<TransformComponent>();
        //     registry.emplace_or_replace<TransformComponent>(entity, transformComp);
        //     if (m_maxZ < transformComp.position.z)
        //         m_maxZ = transformComp.position.z;
        // }
} // namespace Bess
