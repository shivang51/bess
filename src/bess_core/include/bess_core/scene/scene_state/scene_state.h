#pragma once

#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "event_dispatcher.h"

#include <cstdint>
#include <mutex>

namespace Bess::Canvas {
    class SceneState {
      public:
        SceneState();
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
            BESS_ASSERT(component != nullptr,
                        "Cannot add null component to scene");
            if (m_componentsMap.contains(component->getUuid())) {
                BESS_WARN("[SceneState] Component with uuid {} already exists "
                          "in the scene. Skipping addComponent.",
                          (uint64_t)component->getUuid());
                return;
            }

            const auto id = component->getUuid();
            m_componentsMap[id] = component;

            if (component->getParentComponent() == UUID::null) {
                m_rootComponents.insert(id);
            }

            assignRuntimeId(id);

            BESS_DEBUG("[SceneState] Added {} | {} to scene {}",
                       component->getName(),
                       (uint64_t)id,
                       (uint64_t)m_sceneId);

            if (triggerAttach)
                component->onAttach(*this);

            if (dispatchEvent) {
                auto &appCtx = GAppContext::getInstance();
                auto eventDispatcher =
                    appCtx.getSubSystem<EventSystem::EventDispatcher>();
                eventDispatcher->queue(
                    Events::ComponentAddedEvent{.uuid = id,
                                                .type = component->getType(),
                                                .sceneId = m_sceneId,
                                                .state = this});
            }
        }

        // Slightly expensive, but keapt for ease of use.
        template <typename T>
        std::shared_ptr<T> getComponentByUuidSP(const UUID &uuid) const {
            auto itr = m_componentsMap.find(uuid);

            if (itr != m_componentsMap.end()) {
                return dynamic_pointer_cast<T>(itr->second);
            }

            return nullptr;
        }

        // I use it for hotpaths
        template <typename T> T *getComponentByUuid(const UUID &uuid) const {
            auto itr = m_componentsMap.find(uuid);

            if (itr != m_componentsMap.end()) {
                return static_cast<T *>(itr->second.get());
            }

            return nullptr;
        }

        std::shared_ptr<SceneComponent>
        getComponentByUuidSP(const UUID &uuid) const;

        SceneComponent *getComponentByUuid(const UUID &uuid) const;

        std::shared_ptr<SceneComponent>
        getComponentByPickingId(const PickingId &id) const;

        const HashMap<UUID, std::shared_ptr<SceneComponent>> &
        getAllComponents() const;

        const OrderedSet<UUID> &getRootComponents() const;

        MAKE_GETTER_SETTER(UUID, ConnectionStartSlot, m_connectionStartSlot);
        MAKE_GETTER_SETTER(glm::vec2, MousePos, m_mousePos);
        MAKE_GETTER_SETTER(bool, IsRootScene, m_isRootScene);
        MAKE_GETTER_SETTER(UUID, ModuleId, m_moduleId);
        MAKE_GETTER_SETTER(UUID, SceneId, m_sceneId);
        MAKE_GETTER_SETTER(UUID, ParentSceneId, m_parentSceneId);
        MAKE_GETTER_SETTER(std::shared_ptr<UI::UINodeRegistry>,
                           UINodeRegistry,
                           m_uiNodeRegistry);

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

        const HashMap<UUID, bool> &getSelectedComponents() const;

        const UUID &getFocusedUIComponent() const;
        bool isUIComponentFocused(const UUID &uuid) const;
        SceneComponent *getFocusedUIComponentPtr() const;
        bool focusUIComponent(const UUID &uuid,
                              const Events::FocusEvent &event = {});
        void clearUIFocus(const Events::FocusEvent &event = {});

        void attachChild(const UUID &parentId,
                         const UUID &childId,
                         bool emitEvent = true);
        void detachChild(const UUID &childId);

        void assignRuntimeId(const UUID &uuid);

        // Removes a component by UUID from the scene state
        // and all its child components recursively.
        // returns the UUIDs of removed components
        std::vector<UUID> removeComponent(const UUID &uuid,
                                          const UUID &callerId = UUID::null);

        void removeFromMap(const UUID &uuid);

      private:
        const UUID &runtimeIdToUuid(uint32_t runtimeId) const;

      private:
        HashMap<UUID, std::shared_ptr<SceneComponent>> m_componentsMap;
        HashMap<UUID, bool> m_selectedComponents;

        HashMap<uint32_t, UUID> m_runtimeIdMap;
        OrderedSet<UUID> m_rootComponents;
        OrderedSet<uint32_t> m_freeRuntimeIds;

        UUID m_connectionStartSlot = UUID::null;
        UUID m_focusedUIComponent = UUID::null;
        bool m_isRootScene = true;
        UUID m_moduleId = UUID::null; // only used for sub scenes, to know which
                                      // module it belongs to
        UUID m_sceneId, m_parentSceneId = UUID::null;
        glm::vec2 m_mousePos;

        mutable std::mutex m_componentsMutex;

        std::shared_ptr<UI::UINodeRegistry> m_uiNodeRegistry = nullptr;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneState &state, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneState &state);
} // namespace Bess::JsonConvert
