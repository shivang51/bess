#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    void SceneState::clear() {
        m_componentsMap.clear();
        m_typeToUuidsMap.clear();
        m_selectedComponents.clear();
        m_isDraggingComponents = false;
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
        m_componentsMap[uuid]->setIsSelected(true);
    }

    void SceneState::removeSelectedComponent(const UUID &uuid) {
        if (!isComponentValid(uuid))
            return;

        m_selectedComponents[uuid] = false;
        m_componentsMap[uuid]->setIsSelected(false);
    }

    bool SceneState::isComponentSelected(const UUID &uuid) const {
        if (!isComponentValid(uuid) || !m_selectedComponents.contains(uuid))
            return false;

        return m_selectedComponents.at(uuid);
    }

    const std::unordered_map<UUID, bool> &SceneState::getSelectedComponents() const {
        return m_selectedComponents;
    }

    bool SceneState::isDraggingComponents() const {
        return m_isDraggingComponents;
    }

    void SceneState::setIsDraggingComponents(bool dragging) {
        m_isDraggingComponents = dragging;
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
} // namespace Bess::Canvas
