#pragma once

#include "bess_uuid.h"
#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {
    class SceneState {
      public:
        SceneState() = default;
        ~SceneState() = default;

        void clear();

        // T: SceneComponentType = type of scene component
        template <typename T>
        void addComponent(const std::shared_ptr<SceneComponent> &component) {
            const auto casted = component->cast<T>();
            m_componentsMap[casted->getUuid()] = casted;
            m_typeToUuidsMap[casted->getType()].emplace_back(casted->getUuid());
        }

        std::shared_ptr<SceneComponent> getComponentByUuid(const UUID &uuid) const;

        const std::vector<UUID> &getComponentsByType(SceneComponentType type) const;

        const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &getAllComponents() const;

        bool isComponentValid(const UUID &uuid) const;

        void clearSelectedComponents();

        void addSelectedComponent(const UUID &uuid);

        void removeSelectedComponent(const UUID &uuid);

        bool isComponentSelected(const UUID &uuid) const;

        const std::unordered_map<UUID, bool> &getSelectedComponents() const;

        bool isDraggingComponents() const;

        void setIsDraggingComponents(bool dragging);

      private:
        std::unordered_map<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        std::unordered_map<SceneComponentType, std::vector<UUID>> m_typeToUuidsMap;
        std::unordered_map<UUID, bool> m_selectedComponents;
        bool m_isDraggingComponents = false;
    };
} // namespace Bess::Canvas
