#pragma once

#include "bess_uuid.h"
#include "scene/scene_state/components/scene_component.h"
#include <cstdint>

namespace Bess::Canvas {
    class SceneState {
      public:
        SceneState() = default;
        ~SceneState() = default;

        void clear();

        // T: SceneComponentType = type of scene component
        template <typename T>
        void addComponent(const std::shared_ptr<SceneComponent> &component) {
            if (!component ||
                m_componentsMap.contains(component->getUuid()))
                return;

            const auto casted = component->cast<T>();
            m_componentsMap[casted->getUuid()] = casted;
            m_typeToUuidsMap[casted->getType()].emplace_back(casted->getUuid());

            if (component->getParentComponent() == UUID::null) {
                m_rootComponents.emplace_back(casted->getUuid());
            }

            assignRuntimeId(casted->getUuid());
        }

        std::shared_ptr<SceneComponent> getComponentByUuid(const UUID &uuid) const;

        std::shared_ptr<SceneComponent> getComponentByPickingId(const PickingId &id) const;

        const std::vector<UUID> &getComponentsByType(SceneComponentType type) const;

        const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &getAllComponents() const;

        const std::vector<UUID> &getRootComponents() const;

        bool isComponentValid(const UUID &uuid) const;

        void clearSelectedComponents();

        void addSelectedComponent(const UUID &uuid);
        void addSelectedComponent(const PickingId &id);

        void removeSelectedComponent(const UUID &uuid);
        void removeSelectedComponent(const PickingId &id);

        bool isComponentSelected(const UUID &uuid) const;
        bool isComponentSelected(const PickingId &pickingId) const;

        const std::unordered_map<UUID, bool> &getSelectedComponents() const;

        void attachChild(const UUID &parentId, const UUID &childId) const;

        void assignRuntimeId(const UUID &uuid);

      private:
        std::unordered_map<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        std::vector<UUID> m_rootComponents;
        std::unordered_map<SceneComponentType, std::vector<UUID>> m_typeToUuidsMap;
        std::unordered_map<UUID, bool> m_selectedComponents;

        std::unordered_map<uint32_t, UUID> m_runtimeIdMap;
        std::vector<uint32_t> m_freeRuntimeIds;
    };
} // namespace Bess::Canvas
