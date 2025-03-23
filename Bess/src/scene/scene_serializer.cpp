#include "scene/scene_serializer.h"

#include "scene/scene.h"

using namespace Bess::Canvas::Components;

namespace Bess {

    void SceneSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(Canvas::Scene::instance().getEnttRegistry(), path, indent);
    }

    void SceneSerializer::deserializeFromPath(const std::string &path) {
        EnttRegistrySerializer::deserializeFromPath(Canvas::Scene::instance().getEnttRegistry(), path);
    }

    std::string SceneSerializer::serialize(int indent) {
        return EnttRegistrySerializer::serialize(Canvas::Scene::instance().getEnttRegistry(), indent);
    }

    void SceneSerializer::deserialize(const std::string &json) {
        EnttRegistrySerializer::deserialize(Canvas::Scene::instance().getEnttRegistry(), json);
    }

    nlohmann::json SceneSerializer::serializeEntity(entt::registry &registry, entt::entity entity) {
        nlohmann::json j;

        // IdComponent
        if (auto *idComp = registry.try_get<Bess::Canvas::Components::IdComponent>(entity)) {
            j["IdComponent"] = *idComp;
        }

        // TransformComponent
        if (auto *transformComp = registry.try_get<Bess::Canvas::Components::TransformComponent>(entity)) {
            j["TransformComponent"] = *transformComp;
        }

        // SpriteComponent
        if (auto *spriteComp = registry.try_get<Bess::Canvas::Components::SpriteComponent>(entity)) {
            j["SpriteComponent"] = *spriteComp;
        }

        // TagComponent
        if (auto *tagComp = registry.try_get<Bess::Canvas::Components::TagComponent>(entity)) {
            j["TagComponent"] = *tagComp;
        }

        // SlotComponent
        if (auto *slotComp = registry.try_get<Bess::Canvas::Components::SlotComponent>(entity)) {
            j["SlotComponent"] = *slotComp;
        }

        // SimulationComponent
        if (auto *simComp = registry.try_get<Bess::Canvas::Components::SimulationComponent>(entity)) {
            j["SimulationComponent"] = *simComp;
        }

        // ConnectionSegmentComponent
        if (auto *connSegComp = registry.try_get<Bess::Canvas::Components::ConnectionSegmentComponent>(entity)) {
            j["ConnectionSegmentComponent"] = *connSegComp;
        }

        // ConnectionComponent
        if (auto *connComp = registry.try_get<Bess::Canvas::Components::ConnectionComponent>(entity)) {
            j["ConnectionComponent"] = *connComp;
        }

        // SimulationOutputComponent
        if (auto *simOutputComp = registry.try_get<Bess::Canvas::Components::SimulationOutputComponent>(entity)) {
            j["SimulationOutputComponent"] = *simOutputComp;
        }

        // SimulationInputComponent
        if (auto *simInputComp = registry.try_get<Bess::Canvas::Components::SimulationInputComponent>(entity)) {
            j["SimulationInputComponent"] = *simInputComp;
        }

        return j;
    }

    void SceneSerializer::deserializeEntity(entt::registry &registry, const nlohmann::json &j) {
        auto entity = registry.create();

        if (j.contains("IdComponent")) {
            auto idComp = j.at("IdComponent").get<IdComponent>();
            registry.emplace_or_replace<IdComponent>(entity, idComp);
        }

        // TransformComponent
        if (j.contains("TransformComponent")) {
            auto transformComp = j.at("TransformComponent").get<TransformComponent>();
            registry.emplace_or_replace<TransformComponent>(entity, transformComp);
        }

        // SpriteComponent
        if (j.contains("SpriteComponent")) {
            auto spriteComp = j.at("SpriteComponent").get<SpriteComponent>();
            registry.emplace_or_replace<SpriteComponent>(entity, spriteComp);
        }

        // TagComponent
        if (j.contains("TagComponent")) {
            auto tagComp = j.at("TagComponent").get<TagComponent>();
            registry.emplace_or_replace<TagComponent>(entity, tagComp);
        }

        // SlotComponent
        if (j.contains("SlotComponent")) {
            auto slotComp = j.at("SlotComponent").get<SlotComponent>();
            registry.emplace_or_replace<SlotComponent>(entity, slotComp);
        }

        // SimulationComponent
        if (j.contains("SimulationComponent")) {
            auto simComp = j.at("SimulationComponent").get<SimulationComponent>();
            registry.emplace_or_replace<SimulationComponent>(entity, simComp);
        }

        // ConnectionSegmentComponent
        if (j.contains("ConnectionSegmentComponent")) {
            auto connSegComp = j.at("ConnectionSegmentComponent").get<ConnectionSegmentComponent>();
            registry.emplace_or_replace<ConnectionSegmentComponent>(entity, connSegComp);
        }

        // ConnectionComponent
        if (j.contains("ConnectionComponent")) {
            auto connComp = j.at("ConnectionComponent").get<ConnectionComponent>();
            registry.emplace_or_replace<ConnectionComponent>(entity, connComp);
        }

        // SimulationOutputComponent
        if (j.contains("SimulationOutputComponent")) {
            auto simOutputComp = j.at("SimulationOutputComponent").get<SimulationOutputComponent>();
            registry.emplace_or_replace<SimulationOutputComponent>(entity, simOutputComp);
        }

        // SimulationInputComponent
        if (j.contains("SimulationInputComponent")) {
            auto simInputComp = j.at("SimulationInputComponent").get<SimulationInputComponent>();
            registry.emplace_or_replace<SimulationInputComponent>(entity, simInputComp);
        }
    }
} // namespace Bess
