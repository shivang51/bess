#include "scene/scene_state/scene_state.h"
#include "scene/scene_state/components/scene_component.h"
#include <cstdint>

namespace Bess::Canvas {
    void SceneState::clear() {
        m_componentsMap.clear();
        m_typeToUuidsMap.clear();
        m_selectedComponents.clear();
        m_rootComponents.clear();
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

        m_selectedComponents[uuid] = false;
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

    const std::vector<UUID> &SceneState::getRootComponents() const {
        return m_rootComponents;
    }

    void SceneState::attachChild(const UUID &parentId, const UUID &childId) const {
        auto parent = getComponentByUuid(parentId);
        auto child = getComponentByUuid(childId);

        BESS_ASSERT(parent && child, "Parent or child was not found");

        parent->addChildComponent(childId);
        child->setParentComponent(parentId);
    }

    void SceneState::assignRuntimeId(const UUID &uuid) {
        auto component = getComponentByUuid(uuid);
        BESS_ASSERT(component, "Component was not found");

        uint32_t runtimeId = component->getRuntimeId();

        BESS_ASSERT(runtimeId == PickingId::invalidRuntimeId,
                    "Component already has a runtimeId assigned");

        if (!m_freeRuntimeIds.empty()) {
            runtimeId = m_freeRuntimeIds.back();
            m_freeRuntimeIds.pop_back();
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
} // namespace Bess::Canvas
