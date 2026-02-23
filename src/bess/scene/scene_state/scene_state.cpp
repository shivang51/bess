#include "scene/scene_state/scene_state.h"
#include "common/bess_uuid.h"
#include "event_dispatcher.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_ser_reg.h"
#include "simulation_engine.h"
#include <cstdint>
#include <memory>

namespace Bess::Canvas {
    void SceneState::clear() {
        m_runtimeIdMap.clear();
        m_componentsMap.clear();
        m_rootComponents.clear();
        m_freeRuntimeIds.clear();
        m_compConnections.clear();
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

    void SceneState::attachChild(const UUID &parentId, const UUID &childId) {
        auto parent = getComponentByUuid(parentId);
        auto child = getComponentByUuid(childId);

        BESS_ASSERT(parent && child, "Parent or child was not found");

        auto prevParentId = child->getParentComponent();
        if (prevParentId != UUID::null) {
            auto prevParent = getComponentByUuid(prevParentId);
            prevParent->removeChildComponent(childId);
        }

        if (!parent->getChildComponents().contains(childId)) {
            parent->addChildComponent(childId);
        }

        child->setParentComponent(parentId);

        BESS_INFO("[SceneState] Attached component {} to parent component {}",
                  (uint64_t)childId, (uint64_t)parentId);

        EventSystem::EventDispatcher::instance().dispatch(
            Events::EntityReparentedEvent{childId, parentId, prevParentId});

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

        EventSystem::EventDispatcher::instance().dispatch(
            Events::EntityReparentedEvent{childId, UUID::null, parentId});

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

        const auto &connections = getConnectionsForComponent(uuid);
        for (const auto &connectionId : connections) {
            removedUuids.push_back(connectionId);
        }

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
        m_compConnections.erase(uuid);

        if (callerId == UUID::master &&
            component->getParentComponent() != UUID::null) {
            auto parentComp = getComponentByUuid(component->getParentComponent());
            if (parentComp) {
                parentComp->removeChildComponent(uuid);
            }
        }

        EventSystem::EventDispatcher::instance().dispatch(
            Events::ComponentRemovedEvent{uuid,
                                          component->getType()});

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

        EventSystem::EventDispatcher::instance().dispatch(
            Events::EntityReparentedEvent{uuid, UUID::null, parentId});
    }

    bool SceneState::isRootComponent(const UUID &uuid) const {
        return m_rootComponents.contains(uuid);
    }

    void SceneState::addConnectionForComponent(const UUID &compId, const UUID &connectionId) {
        m_compConnections[compId].push_back(connectionId);
    }

    void SceneState::removeConnectionForComponent(const UUID &compId, const UUID &connectionId) {
        BESS_DEBUG("[SceneState] Removing connection {} for component {}",
                   (uint64_t)connectionId, (uint64_t)compId);
        if (!m_compConnections.contains(compId)) {
            return;
        }

        auto &connections = m_compConnections[compId];
        connections.erase(std::ranges::remove(connections, connectionId).begin(),
                          connections.end());

        if (connections.empty()) {
            m_compConnections.erase(compId);
        }
    }

    const std::vector<UUID> &SceneState::getConnectionsForComponent(const UUID &compId) const {
        if (!m_compConnections.contains(compId)) {
            static const std::vector<UUID> emptyConnections;
            return emptyConnections;
        }

        return m_compConnections.at(compId);
    }

    const std::unordered_map<UUID, std::vector<UUID>> &SceneState::getAllComponentConnections() const {
        return m_compConnections;
    }

    SceneState::SceneState(const SceneState &scene) {
        BESS_ERROR("SceneState copy is not allowed");
        BESS_ASSERT(false, "SceneState copy is not allowed");
    }

    std::set<UUID> SceneState::getLifeDependants(const UUID &uuid) const {
        std::set<UUID> dependants;
        const auto &comp = getComponentByUuid(uuid);
        if (!comp) {
            return dependants;
        }

        for (const auto &childId : comp->getChildComponents()) {
            dependants.insert(childId);
            const auto &childDependants = getLifeDependants(childId);
            dependants.insert(childDependants.begin(), childDependants.end());
        }

        const auto &connections = getConnectionsForComponent(uuid);
        for (const auto &connId : connections) {
            dependants.insert(connId);
            const auto &connDependants = getLifeDependants(connId);
            dependants.insert(connDependants.begin(), connDependants.end());
        }

        return dependants;
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j) {
        j["components"] = Json::Value(Json::arrayValue);

        for (const auto &[uuid, component] : state.getAllComponents()) {
            j["components"].append(component->toJson());
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state) {
        state.clear();

        if (!j.isMember("components") || !j["components"].isArray()) {
            return;
        }

        const auto &simEngine = SimEngine::SimulationEngine::instance();

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

            if (comp->getType() == Canvas::SceneComponentType::simulation) {
                const auto &simComp = comp->cast<Canvas::SimulationSceneComponent>();
                const auto &def = simEngine.getComponentDefinition(simComp->getSimEngineId());
                simComp->setCompDef(def->clone());
            }

            state.addComponent(comp, false, false);
            BESS_DEBUG("Added component {} with UUID {} from JSON", comp->getName(), (uint64_t)comp->getUuid());
        }
    }

} // namespace Bess::JsonConvert
