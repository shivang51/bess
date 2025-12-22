#include "scene/scene_state/scene_state.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/input_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include <cstdint>

namespace Bess::Canvas {
    void SceneState::clear() {
        m_componentsMap.clear();
        m_typeToUuidsMap.clear();
        m_selectedComponents.clear();
        m_rootComponents.clear();
        m_runtimeIdMap.clear();
        m_freeRuntimeIds.clear();
        m_slotsConnectionMap.clear();
    }

    std::shared_ptr<SceneComponent> SceneState::getComponentByUuid(const UUID &uuid) const {
        if (m_componentsMap.contains(uuid)) {
            return m_componentsMap.at(uuid);
        }
        return nullptr;
    }

    const std::vector<UUID> &SceneState::getComponentsByType(SceneComponentType type) const {
        static const std::vector<UUID> empty;
        auto it = m_typeToUuidsMap.find(type);
        return it != m_typeToUuidsMap.end() ? it->second : empty;
    }

    const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &SceneState::getAllComponents() const {
        return m_componentsMap;
    }

    bool SceneState::isComponentValid(const UUID &uuid) const {
        return m_componentsMap.contains(uuid);
    }

    void SceneState::clearSelectedComponents() {
        for (const auto &[uuid, selected] : m_selectedComponents) {
            m_componentsMap[uuid]->setIsSelected(false);
        }
        m_selectedComponents.clear();
    }

    void SceneState::addSelectedComponent(const UUID &uuid) {
        if (!isComponentValid(uuid))
            return;

        m_selectedComponents[uuid] = true;
        m_componentsMap.at(uuid)->setIsSelected(true);
    }

    void SceneState::addSelectedComponent(const PickingId &id) {
        addSelectedComponent(m_runtimeIdMap.at(id.runtimeId));
    }

    void SceneState::removeSelectedComponent(const UUID &uuid) {
        if (!isComponentValid(uuid))
            return;

        m_selectedComponents.erase(uuid);
        m_componentsMap.at(uuid)->setIsSelected(false);
    }

    void SceneState::removeSelectedComponent(const PickingId &id) {
        removeSelectedComponent(m_runtimeIdMap.at(id.runtimeId));
    }

    bool SceneState::isComponentSelected(const UUID &uuid) const {
        if (!isComponentValid(uuid) || !m_selectedComponents.contains(uuid))
            return false;

        return m_selectedComponents.at(uuid);
    }

    bool SceneState::isComponentSelected(const PickingId &pickingId) const {
        return isComponentSelected(m_runtimeIdMap.at(pickingId.runtimeId));
    }

    const std::unordered_map<UUID, bool> &SceneState::getSelectedComponents() const {
        return m_selectedComponents;
    }

    const std::unordered_set<UUID> &SceneState::getRootComponents() const {
        return m_rootComponents;
    }

    void SceneState::attachChild(const UUID &parentId, const UUID &childId) {
        auto parent = getComponentByUuid(parentId);
        auto child = getComponentByUuid(childId);

        BESS_ASSERT(parent && child, "Parent or child was not found");

        parent->addChildComponent(childId);
        child->setParentComponent(parentId);

        m_rootComponents.erase(childId);
    }

    void SceneState::assignRuntimeId(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        uint32_t runtimeId = component->getRuntimeId();

        BESS_ASSERT(runtimeId == PickingId::invalidRuntimeId,
                    "Component already has a runtimeId assigned");

        if (!m_freeRuntimeIds.empty()) {
            runtimeId = *m_freeRuntimeIds.rbegin();
            m_freeRuntimeIds.erase(runtimeId);
            BESS_ASSERT(m_runtimeIdMap.at(runtimeId) == UUID::null,
                        "Reusing runtimeId that is still mapped");
        } else {
            runtimeId = static_cast<uint32_t>(m_runtimeIdMap.size());
        }

        component->setRuntimeId(runtimeId);
        m_runtimeIdMap[runtimeId] = uuid;
    }

    std::shared_ptr<SceneComponent> SceneState::getComponentByPickingId(const PickingId &id) const {
        if (!m_runtimeIdMap.contains(id.runtimeId)) {
            return nullptr;
        }

        const auto &uuid = m_runtimeIdMap.at(id.runtimeId);
        return getComponentByUuid(uuid);
    }

    std::vector<UUID> SceneState::removeComponent(const UUID &uuid, const UUID &callerId) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        /// For now, Preventing removing child components directly
        /// If parent is not the caller, then do not remove
        /// TODO(Shivang): Add lifetime ownership management later
        if (component->getParentComponent() != callerId) {
            return {};
        }

        std::vector<UUID> removedUuids = {uuid};

        for (const auto &childUuid : component->getChildComponents()) {
            auto childComp = getComponentByUuid(childUuid);
            auto ids = removeComponent(childUuid, uuid);
            removedUuids.insert(removedUuids.end(), ids.begin(), ids.end());
        }

        const uint32_t runtimeId = component->getRuntimeId();
        if (runtimeId != PickingId::invalidRuntimeId) {
            m_runtimeIdMap[runtimeId] = UUID::null;
            m_freeRuntimeIds.insert(runtimeId);
        }

        removeSelectedComponent(uuid);

        auto &typeVec = m_typeToUuidsMap[component->getType()];
        typeVec.erase(std::ranges::remove(typeVec, uuid).begin(), typeVec.end());

        if (component->getParentComponent() == UUID::null) {
            m_rootComponents.erase(uuid);
        }

        m_componentsMap.erase(uuid);

        return removedUuids;
    }

    void SceneState::addSlotConnMapping(const std::string &slotKey, const UUID &connId) {
        m_slotsConnectionMap[slotKey] = connId;
    }

    void SceneState::removeSlotConnMapping(const std::string &slotKey) {
        m_slotsConnectionMap.erase(slotKey);
    }

    UUID SceneState::getConnBetweenSlots(const UUID &slotA, const UUID &slotB) const {
        const auto slotAComp = getComponentByUuid<SlotSceneComponent>(slotA);
        const auto slotBComp = getComponentByUuid<SlotSceneComponent>(slotB);

        if (slotAComp->getSlotType() == SlotType::digitalInput) {
            const std::string key1 = slotA.toString() + "-" + slotB.toString();
            if (m_slotsConnectionMap.contains(key1)) {
                return m_slotsConnectionMap.at(key1);
            }
        } else {
            const std::string key2 = slotB.toString() + "-" + slotA.toString();
            if (m_slotsConnectionMap.contains(key2)) {
                return m_slotsConnectionMap.at(key2);
            }
        }

        return UUID::null;
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j) {
        j["components"] = Json::Value(Json::arrayValue);

        for (const auto &[uuid, component] : state.getAllComponents()) {
            j["components"].append(Json::Value());
            auto &compJson = j["components"][static_cast<int>(j["components"].size() - 1)];
            switch (component->getType()) {
            case Canvas::SceneComponentType::simulation: {
                if (auto inputComp = component->cast<Canvas::InputSceneComponent>()) {
                    toJsonValue(*inputComp, compJson);
                } else {
                    toJsonValue(*component->cast<Canvas::SimulationSceneComponent>(),
                                compJson);
                }
                break;
            }
            case Canvas::SceneComponentType::slot: {
                toJsonValue(*component->cast<Canvas::SlotSceneComponent>(), compJson);
                break;
            }
            case Canvas::SceneComponentType::connection: {
                toJsonValue(*component->cast<Canvas::ConnectionSceneComponent>(), compJson);
                break;
            }
            default: {
                toJsonValue(*component, compJson);
                break;
            }
            }
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state) {
        state.clear();

        if (!j.isMember("components") || !j["components"].isArray()) {
            return;
        }

        for (const auto &compJson : j["components"]) {
            if (!compJson.isMember("type")) {
                continue;
            }

            Canvas::SceneComponentType type =
                static_cast<Canvas::SceneComponentType>(compJson["type"].asInt());

            switch (type) {
            case Canvas::SceneComponentType::simulation: {
                std::string subType;
                if (compJson.isMember("subType")) {
                    subType = compJson["subType"].asString();
                }
                if (subType == Canvas::InputSceneComponent::subType) {
                    Canvas::InputSceneComponent inputComp;
                    fromJsonValue(compJson, inputComp);
                    state.addComponent<Canvas::InputSceneComponent>(
                        std::make_shared<Canvas::InputSceneComponent>(inputComp));
                } else {
                    Canvas::SimulationSceneComponent simComp;
                    fromJsonValue(compJson, simComp);
                    state.addComponent<Canvas::SimulationSceneComponent>(
                        std::make_shared<Canvas::SimulationSceneComponent>(simComp));
                }
                break;
            }
            case Canvas::SceneComponentType::slot: {
                Canvas::SlotSceneComponent slotComp;
                fromJsonValue(compJson, slotComp);
                state.addComponent<Canvas::SlotSceneComponent>(
                    std::make_shared<Canvas::SlotSceneComponent>(slotComp));
                break;
            }
            case Canvas::SceneComponentType::connection: {
                Canvas::ConnectionSceneComponent connComp;
                fromJsonValue(compJson, connComp);
                state.addComponent<Canvas::ConnectionSceneComponent>(
                    std::make_shared<Canvas::ConnectionSceneComponent>(connComp));
                break;
            }
            default: {
                Canvas::SceneComponent baseComp;
                fromJsonValue(compJson, baseComp);
                state.addComponent<Canvas::SceneComponent>(std::make_shared<Canvas::SceneComponent>(baseComp));
                break;
            }
            }
        }
    }

} // namespace Bess::JsonConvert
