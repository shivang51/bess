#include "scene/scene_serializer.h"

#include <algorithm>

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/scene.h"

using namespace Bess::Canvas::Components;

namespace Bess {

    SceneSerializer::SceneSerializer() {
        SceneSerializer::registerAll();
    }

    void SceneSerializer::serializeToPath(const std::string &path, int indent) {
        EnttRegistrySerializer::serializeToPath(Canvas::Scene::instance()->getEnttRegistry(), path, indent);
    }

    void SceneSerializer::deserializeFromPath(const std::string &path) {
        auto &reg = Canvas::Scene::instance()->getEnttRegistry();
        reg.clear();
        EnttRegistrySerializer::deserializeFromPath(reg, path);
    }

    void SceneSerializer::serialize(Json::Value &j) {
        EnttRegistrySerializer::serialize(Canvas::Scene::instance()->getEnttRegistry(), j);
    }

    void SceneSerializer::serializeEntity(UUID uid, Json::Value &j) {
        const auto ent = Canvas::Scene::instance()->getEntityWithUuid(uid);
        auto &reg = Canvas::Scene::instance()->getEnttRegistry();
        EnttRegistrySerializer::serializeEntity(reg, ent, j["components"]);

        if (const auto *simComp = reg.try_get<SimulationComponent>(ent)) {
            for (const auto &slot : simComp->inputSlots)
                serializeEntity(slot, j["slots"].append(Json::objectValue));

            for (const auto &slot : simComp->outputSlots)
                serializeEntity(slot, j["slots"].append(Json::objectValue));
        }

        if (const auto *slotComp = reg.try_get<SlotComponent>(ent)) {
            for (const auto &conn : slotComp->connections) {
                serializeEntity(conn, j["connections"].append(Json::objectValue));
            }
        }

        if (const auto *connComp = reg.try_get<ConnectionComponent>(ent)) {
            UUID segId = connComp->segmentHead;
            while (segId != UUID::null) {
                const auto &seg = reg.get<ConnectionSegmentComponent>(
                    Canvas::Scene::instance()->getEntityWithUuid(segId));

                serializeEntity(segId, j["segments"].append(Json::objectValue));

                segId = seg.next;
            }
        }
    }

    void SceneSerializer::deserialize(const Json::Value &json) {
        m_maxZ = 0;

        auto scene = Canvas::Scene::instance();
        scene->clear();

        auto &reg = scene->getEnttRegistry();
        EnttRegistrySerializer::deserialize(reg, json);

        for (auto entity : reg.view<TransformComponent>()) {
            const auto &transform = reg.get<TransformComponent>(entity);
            m_maxZ = std::max(transform.position.z, m_maxZ);
        }

        scene->setZCoord(m_maxZ);
    }

    void SceneSerializer::deserializeEntity(const Json::Value &json) {
        auto &registry = Canvas::Scene::instance()->getEnttRegistry();
        entt::entity entity = EnttRegistrySerializer::deserializeEntity(registry, json["components"]);

        if (registry.valid(entity)) {
            bool isSim = registry.any_of<SimulationComponent>(entity);
            bool isNS = registry.any_of<NSComponent>(entity);

            if (isSim || isNS) {
                if (registry.any_of<IdComponent>(entity)) {
                    UUID uuid = registry.get<IdComponent>(entity).uuid;
                    // Canvas::Scene::instance()->getEventDispatcher().trigger(Events::ComponentCreatedEvent{uuid, entity, isSim});
                }
            }
        }

        if (json.isMember("slots")) {
            for (auto &slot : json["slots"]) {
                deserializeEntity(slot);
            }
        }

        if (json.isMember("connections")) {
            for (auto &conn : json["connections"]) {
                deserializeEntity(conn);
            }
        }

        if (json.isMember("segments")) {
            for (auto &seg : json["segments"]) {
                deserializeEntity(seg);
            }
        }
    }

    void SceneSerializer::registerAll() {
        registerComponent<SimEngine::IdComponent>("IdComponent");
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
        registerComponent<NSComponent>("NSComponent");
    }
} // namespace Bess
