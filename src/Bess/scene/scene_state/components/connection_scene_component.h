#pragma once

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/types.h"

namespace Bess::Canvas {

    class ConnectionSceneComponent : public SceneComponent,
                                     public DragBehaviour<ConnectionSceneComponent> {
      public:
        static std::string genSlotsKey(const SceneState &state, const UUID &slotA, const UUID &slotB);
        static std::pair<UUID, UUID> parseSlotsKey(const std::string &key);

        ConnectionSceneComponent() = default;
        ConnectionSceneComponent(const ConnectionSceneComponent &other) = default;
        ConnectionSceneComponent(UUID uuid);

        ConnectionSceneComponent(UUID uuid, const Transform &transform);
        ~ConnectionSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void onMouseDragged(const Events::MouseDraggedEvent &e) override;

        void onMouseDragBegin(const Events::MouseDraggedEvent &e) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        REG_SCENE_COMP(SceneComponentType::connection)

        MAKE_GETTER(UUID, StartSlot, m_startSlot)
        MAKE_GETTER(UUID, EndSlot, m_endSlot)
        MAKE_GETTER_SETTER(std::vector<ConnSegment>, Segments, m_segments)

        void setStartEndSlots(const UUID &startSlot, const UUID &endSlot);

        void reconsturctSegments(const SceneState &state);

        std::string getSlotsKey() const;

      private:
        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> pathRenderer) override;

        void calculateKey(SceneState &state);

        UUID m_startSlot = UUID::null;
        UUID m_endSlot = UUID::null;
        std::vector<ConnSegment> m_segments;
        int m_draggedSegIdx = -1;
        int m_hoveredSegIdx = -1;

        // String of <inputSlotUUID>-<outputSlotUUID>
        // Depends on which slot is start and which is end
        // To store the which slots are connected with which connection
        // Will be used by scene state in a unordered_map<std::string, UUID>
        std::string m_slotsKey;
        bool m_isKeyDirty = true;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::ConnectionSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnectionSceneComponent &component);
} // namespace Bess::JsonConvert
