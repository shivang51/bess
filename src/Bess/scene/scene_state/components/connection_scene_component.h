#pragma once

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/renderer/vulkan/path_renderer.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/scene_state.h"

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

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        REG_SCENE_COMP(SceneComponentType::connection)

        MAKE_GETTER(UUID, StartSlot, m_startSlot)
        MAKE_GETTER(UUID, EndSlot, m_endSlot)
        MAKE_GETTER_SETTER(std::vector<ConnSegment>, Segments, m_segments)
        MAKE_GETTER_SETTER(bool, ShouldReconstructSegments, m_shouldReconstructSegments)
        MAKE_GETTER_SETTER(bool, UseCustomColor, m_useCustomColor)
        MAKE_GETTER_SETTER(int, InitialSegmentCount, m_initialSegmentCount)

        void setStartEndSlots(const UUID &startSlot, const UUID &endSlot);

        void reconstructSegments(const SceneState &state);

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        glm::vec3 getSegVertexPos(const SceneState &state, size_t vertexIdx);

      private:
        void onFirstDraw(SceneState &sceneState,
                         std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<PathRenderer> pathRenderer) override;

        void drawSegments(const SceneState &state,
                          const glm::vec3 &startPos,
                          const glm::vec3 &endPos,
                          const glm::vec4 &color,
                          const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer);

        void resetSegmentPositionCache(const SceneState &state);
        UUID m_startSlot = UUID::null;
        UUID m_endSlot = UUID::null;
        std::vector<ConnSegment> m_segments;
        std::vector<glm::vec3> m_segmentCachedPositions;
        bool m_segmentPosCacheDirty = true;
        int m_draggedSegIdx = -1;
        int m_hoveredSegIdx = -1;
        bool m_useCustomColor = false;
        int m_initialSegmentCount = 3;
        bool m_shouldReconstructSegments = false;
    };
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::ConnectionSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnectionSceneComponent &component);
} // namespace Bess::JsonConvert
