#pragma once

#include "bess_uuid.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include <cstdint>
#include <unordered_set>

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

            const auto id = component->getUuid();
            m_componentsMap[id] = component;
            m_typeToUuidsMap[component->getType()].emplace_back(id);

            if (component->getParentComponent() == UUID::null) {
                m_rootComponents.insert(id);
            }

            if (component->getType() == SceneComponentType::simulation) {
                auto simComp = component->cast<SimulationSceneComponent>();
                m_simEngineIdToSceneCompMap[simComp->getSimEngineId()] = id;
            }

            assignRuntimeId(id);
        }

        template <typename T>
        std::shared_ptr<T> getComponentByUuid(const UUID &uuid) const {
            if (m_componentsMap.contains(uuid)) {
                return m_componentsMap.at(uuid)->cast<T>();
            }
            return nullptr;
        }

        template <typename T>
        std::shared_ptr<T> getComponentBySimEngineId(const UUID &simEngineId) const {
            auto it = m_simEngineIdToSceneCompMap.find(simEngineId);
            if (it == m_simEngineIdToSceneCompMap.end()) {
                return nullptr;
            }
            return getComponentByUuid<T>(it->second);
        }

        std::shared_ptr<SceneComponent> getComponentByUuid(const UUID &uuid) const;

        std::shared_ptr<SceneComponent> getComponentByPickingId(const PickingId &id) const;

        const std::vector<UUID> &getComponentsByType(SceneComponentType type) const;

        const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &getAllComponents() const;

        const std::unordered_set<UUID> &getRootComponents() const;

        bool isComponentValid(const UUID &uuid) const;

        void clearSelectedComponents();

        void addSelectedComponent(const UUID &uuid);
        void addSelectedComponent(const PickingId &id);

        void removeSelectedComponent(const UUID &uuid);
        void removeSelectedComponent(const PickingId &id);

        bool isComponentSelected(const UUID &uuid) const;
        bool isComponentSelected(const PickingId &pickingId) const;

        const std::unordered_map<UUID, bool> &getSelectedComponents() const;

        void removeSlotConnMapping(const std::string &slotKey);
        void addSlotConnMapping(const std::string &slotKey, const UUID &connId);
        UUID getConnBetweenSlots(const UUID &slotA, const UUID &slotB) const;

        void attachChild(const UUID &parentId, const UUID &childId);
        void assignRuntimeId(const UUID &uuid);

        // Removes a component by UUID from the scene state
        // and all its child components recursively.
        // returns the UUIDs of removed components
        std::vector<UUID> removeComponent(const UUID &uuid, const UUID &callerId = UUID::null);

      private:
        std::unordered_map<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        std::unordered_map<SceneComponentType, std::vector<UUID>> m_typeToUuidsMap;
        std::unordered_map<UUID, UUID> m_simEngineIdToSceneCompMap;
        std::unordered_map<UUID, bool> m_selectedComponents;
        std::unordered_map<std::string, UUID> m_slotsConnectionMap;
        std::unordered_map<uint32_t, UUID> m_runtimeIdMap;

        std::unordered_set<UUID> m_rootComponents;
        std::set<uint32_t> m_freeRuntimeIds;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state);
} // namespace Bess::JsonConvert
