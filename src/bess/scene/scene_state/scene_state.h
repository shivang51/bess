#pragma once

#include "common/bess_uuid.h"
#include "event_dispatcher.h"
#include "scene/scene_state/components/scene_component.h"
#include <cstdint>
#include <set>
#include <unordered_set>

namespace Bess::Canvas {
    class SceneState {
      public:
        SceneState() = default;
        ~SceneState() = default;

        SceneState(const SceneState &);
        SceneState &operator=(const SceneState &) = delete;
        SceneState(SceneState &&) = delete;

        void clear();

        // T: SceneComponentType = type of scene component
        template <typename T>
        void addComponent(const std::shared_ptr<T> &component,
                          bool triggerAttach = true,
                          bool dispatchEvent = true) {
            static_assert(std::is_base_of<Bess::Canvas::SceneComponent, T>(),
                          "T must be derived from SceneComponent");

            if (!component ||
                m_componentsMap.contains(component->getUuid())) {
                BESS_WARN("[SceneState] Component with uuid {} already exists in the scene. Skipping addComponent.",
                          (uint64_t)component->getUuid());
                return;
            }

            const auto id = component->getUuid();
            m_componentsMap[id] = component;

            if (component->getParentComponent() == UUID::null) {
                m_rootComponents.insert(id);
            }

            assignRuntimeId(id);

            if (triggerAttach)
                component->onAttach(*this);

            if (dispatchEvent)
                EventSystem::EventDispatcher::instance().queue(
                    Events::ComponentAddedEvent{.uuid = id,
                                                .type = component->getType()});
        }

        template <typename T>
        std::shared_ptr<T> getComponentByUuid(const UUID &uuid) const {
            if (m_componentsMap.contains(uuid)) {
                return m_componentsMap.at(uuid)->cast<T>();
            }
            return nullptr;
        }

        std::shared_ptr<SceneComponent> getComponentByUuid(const UUID &uuid) const;

        std::shared_ptr<SceneComponent> getComponentByPickingId(const PickingId &id) const;

        const std::unordered_map<UUID, std::shared_ptr<SceneComponent>> &getAllComponents() const;

        const std::unordered_set<UUID> &getRootComponents() const;

        MAKE_GETTER_SETTER(bool, IsSchematicView, m_isSchematicView);
        MAKE_GETTER_SETTER(UUID, ConnectionStartSlot, m_connectionStartSlot);
        MAKE_GETTER_SETTER(glm::vec2, MousePos, m_mousePos);

        // Removes the parent reference of the component,
        // but keeps this component in parents children list,
        // So its still parents child but parent is null(dead),
        // so this component becomes the root component
        void orphanComponent(const UUID &uuid);

        bool isComponentValid(const UUID &uuid) const;

        void clearSelectedComponents();

        void addSelectedComponent(const UUID &uuid);
        void addSelectedComponent(const PickingId &id);

        void removeSelectedComponent(const UUID &uuid);
        void removeSelectedComponent(const PickingId &id);

        bool isComponentSelected(const UUID &uuid) const;
        bool isComponentSelected(const PickingId &pickingId) const;

        bool isRootComponent(const UUID &uuid) const;

        const std::unordered_map<UUID, bool> &getSelectedComponents() const;

        void attachChild(const UUID &parentId, const UUID &childId);
        void detachChild(const UUID &childId);

        void assignRuntimeId(const UUID &uuid);

        // Removes a component by UUID from the scene state
        // and all its child components recursively.
        // returns the UUIDs of removed components
        std::vector<UUID> removeComponent(const UUID &uuid, const UUID &callerId = UUID::null);

      private:
        std::unordered_map<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        std::unordered_map<UUID, bool> m_selectedComponents;

        std::unordered_map<uint32_t, UUID> m_runtimeIdMap;
        std::unordered_set<UUID> m_rootComponents;
        std::set<uint32_t> m_freeRuntimeIds;

        UUID m_connectionStartSlot = UUID::null;
        bool m_isSchematicView = false;
        glm::vec2 m_mousePos;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state);
} // namespace Bess::JsonConvert
