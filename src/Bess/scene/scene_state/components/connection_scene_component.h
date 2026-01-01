#pragma once

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"

namespace Bess::Canvas {

    class ConnectionSceneComponent : public SceneComponent,
                                     public DragBehaviour<ConnectionSceneComponent> {
      public:
        ConnectionSceneComponent();

        ~ConnectionSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawSchematic(SceneState &state,
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
        MAKE_GETTER_SETTER(bool, UseCustomColor, m_useCustomColor)

        void setStartEndSlots(const UUID &startSlot, const UUID &endSlot);

        void reconsturctSegments(const SceneState &state);

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

      private:
        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> pathRenderer) override;

        void drawSegments(const SceneState &state,
                          const glm::vec3 &startPos,
                          const glm::vec3 &endPos,
                          const glm::vec4 &color,
                          std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer);

        UUID m_startSlot = UUID::null;
        UUID m_endSlot = UUID::null;
        std::vector<ConnSegment> m_segments;
        int m_draggedSegIdx = -1;
        int m_hoveredSegIdx = -1;
        bool m_useCustomColor = false;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::ConnectionSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnectionSceneComponent &component);
} // namespace Bess::JsonConvert
