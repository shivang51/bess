#include "scene/scene_state/scene_state.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_ser_reg.h"
#include "simulation_engine.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Bess::Canvas {
    void SceneState::clear() {
        m_runtimeIdMap.clear();
        m_componentsMap.clear();
        m_rootComponents.clear();
        m_freeRuntimeIds.clear();
        m_selectedComponents.clear();
    }

    std::shared_ptr<SceneComponent> SceneState::getComponentByUuid(const UUID &uuid) const {
        if (m_componentsMap.contains(uuid)) {
            return m_componentsMap.at(uuid);
        }
        return nullptr;
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
        if (id.info & PickingId::InfoFlags::unSelectable) {
            return;
        }

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

    void SceneState::attachChild(const UUID &parentId, const UUID &childId, bool emitEvent) {
        auto parent = getComponentByUuid(parentId);
        auto child = getComponentByUuid(childId);

        BESS_ASSERT(parent, "Parent was not found");
        BESS_ASSERT(child, "Child was not found");

        auto prevParentId = child->getParentComponent();
        if (prevParentId != UUID::null && prevParentId != parentId) {
            auto prevParent = getComponentByUuid(prevParentId);
            prevParent->removeChildComponent(childId);
        }

        if (!parent->getChildComponents().contains(childId)) {
            parent->addChildComponent(childId);
        }

        child->setParentComponent(parentId);

        BESS_INFO("[SceneState] Attached component {} to parent component {}",
                  (uint64_t)childId, (uint64_t)parentId);

        if (emitEvent) {
            EventSystem::EventDispatcher::instance().queue(
                Events::EntityReparentedEvent{childId, parentId, prevParentId, this});
        }

        m_rootComponents.erase(childId);
    }

    void SceneState::detachChild(const UUID &childId) {
        const auto &child = getComponentByUuid(childId);
        BESS_ASSERT(child, "(Detach Child) Child not found");

        const auto parentId = child->getParentComponent();
        if (parentId == UUID::null) {
            return;
        }

        const auto &parent = getComponentByUuid(parentId);

        parent->removeChildComponent(childId);
        child->setParentComponent(UUID::null);

        BESS_INFO("[SceneState] Detached component {} from parent component {}",
                  (uint64_t)childId, (uint64_t)parentId);

        EventSystem::EventDispatcher::instance().queue(
            Events::EntityReparentedEvent{childId, UUID::null, parentId, this});

        m_rootComponents.insert(childId);
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

    std::shared_ptr<SceneComponent> SceneState::getComponentByPickingId(
        const PickingId &id) const {
        if (!m_runtimeIdMap.contains(id.runtimeId)) {
            return nullptr;
        }

        const auto &uuid = m_runtimeIdMap.at(id.runtimeId);
        return getComponentByUuid(uuid);
    }

    std::vector<UUID> SceneState::removeComponent(const UUID &uuid, const UUID &callerId) {
        BESS_INFO("[SceneState] Removing component {}", (uint64_t)uuid);
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        /// For now, Preventing removing child components directly
        /// If parent is not the caller, then do not remove
        /// TODO(Shivang): Add lifetime ownership management later
        if (component->getParentComponent() != UUID::null &&
            callerId != UUID::master &&
            component->getParentComponent() != callerId) {
            BESS_WARN("[SceneState] Attempt to remove child component {} directly prevented",
                      (uint64_t)uuid);
            return {};
        }

        std::vector<UUID> removedUuids = component->cleanup(*this, callerId);
        removedUuids.push_back(uuid);

        const uint32_t runtimeId = component->getRuntimeId();
        if (runtimeId != PickingId::invalidRuntimeId) {
            component->setRuntimeId(PickingId::invalidRuntimeId); // Don't remove this its not redundant
            m_runtimeIdMap[runtimeId] = UUID::null;
            m_freeRuntimeIds.insert(runtimeId);
        }

        removeSelectedComponent(uuid);

        if (component->getParentComponent() == UUID::null) {
            m_rootComponents.erase(uuid);
        }

        m_componentsMap.erase(uuid);

        if (callerId == UUID::master &&
            component->getParentComponent() != UUID::null) {
            auto parentComp = getComponentByUuid(component->getParentComponent());
            if (parentComp) {
                parentComp->removeChildComponent(uuid);
            }
        }

        EventSystem::EventDispatcher::instance().queue(
            Events::ComponentRemovedEvent{uuid,
                                          component->getType(),
                                          this});

        return removedUuids;
    }

    void SceneState::orphanComponent(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        auto parentId = component->getParentComponent();
        if (parentId != UUID::null) {
            component->setParentComponent(UUID::null);
            m_rootComponents.insert(uuid);
        }

        BESS_INFO("[SceneState] Orphaned component {}", (uint64_t)uuid);

        EventSystem::EventDispatcher::instance().queue(
            Events::EntityReparentedEvent{uuid, UUID::null, parentId, this});
    }

    bool SceneState::isRootComponent(const UUID &uuid) const {
        return m_rootComponents.contains(uuid);
    }

    SceneState::SceneState(const SceneState &scene) {
        BESS_ERROR("SceneState copy is not allowed");
        BESS_ASSERT(false, "SceneState copy is not allowed");
    }

    void SceneState::removeFromMap(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        const uint32_t runtimeId = component->getRuntimeId();
        if (runtimeId != PickingId::invalidRuntimeId) {
            component->setRuntimeId(PickingId::invalidRuntimeId); // Don't remove this its not redundant
            m_runtimeIdMap[runtimeId] = UUID::null;
            m_freeRuntimeIds.insert(runtimeId);
        }

        m_rootComponents.erase(uuid);
        m_selectedComponents.erase(uuid);
        m_componentsMap.erase(uuid);
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j) {
        j["components"] = Json::Value(Json::arrayValue);

        for (const auto &[uuid, component] : state.getAllComponents()) {
            j["components"].append(component->toJson());
        }

        JsonConvert::toJsonValue(state.getSceneId(), j["sceneId"]);
        JsonConvert::toJsonValue(state.getModuleId(), j["moduleId"]);
        j["isRootScene"] = state.getIsRootScene();
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state) {
        state.clear();

        if (!j.isMember("components") || !j["components"].isArray()) {
            return;
        }

        JsonConvert::fromJsonValue(j["sceneId"], state.getSceneId());
        JsonConvert::fromJsonValue(j["moduleId"], state.getModuleId());
        state.setIsRootScene(j["isRootScene"].asBool());

        const auto &simEngine = SimEngine::SimulationEngine::instance();
        std::vector<std::shared_ptr<Canvas::SceneComponent>> deserializedComponents;
        deserializedComponents.reserve(j["components"].size());

        for (const auto &compJson : j["components"]) {
            if (!compJson.isMember("typeName")) {
                BESS_WARN("Component JSON is missing typeName field. Skipping component.");
                continue;
            }

            auto comp = Canvas::SceneSerReg::createComponentFromJson(compJson);
            if (!comp) {
                BESS_WARN("Failed to create component from JSON. Skipping component.");
                continue;
            }

            if (comp->getType() == Canvas::SceneComponentType::module ||
                comp->getType() == Canvas::SceneComponentType::simulation) {
                const auto &simComp = comp->cast<Canvas::SimulationSceneComponent>();
                BESS_ASSERT(simComp, "Simulation component cast failed");
                const auto &def = simEngine.getComponentDefinition(simComp->getSimEngineId());
                BESS_ASSERT(def, "Definition not found in sim engine");
                simComp->setCompDef(def);
                simComp->setScaleDirty(false);
            }

            state.addComponent(comp, false, false);
            deserializedComponents.push_back(comp);
        }

        for (const auto &comp : deserializedComponents) {
            comp->onAttach(state);
        }
    }

} // namespace Bess::JsonConvert
