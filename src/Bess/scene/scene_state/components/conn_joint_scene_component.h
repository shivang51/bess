#pragma once
#include "bess_uuid.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {

    class ConnJointSceneComp : public SceneComponent,
                               public DragBehaviour<ConnJointSceneComp> {
      public:
        ConnJointSceneComp() = default;
        ConnJointSceneComp(UUID connectionId, int connSegIdx, ConnSegOrientaion segOrientation);

        REG_SCENE_COMP(SceneComponentType::connJoint)
        MAKE_GETTER_SETTER(int, ConnSegIdx, m_connSegIdx);
        MAKE_GETTER_SETTER(UUID, ConnectionId, m_connectionId);

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawSchematic(SceneState &state,
                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void onMouseDragged(const Events::MouseDraggedEvent &e) override;
        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        glm::vec3 getAbsolutePosition(const SceneState &state) const override;

      private:
        int m_connSegIdx;
        UUID m_connectionId;
        float m_segOffset = 0.5f; // normalized 0-1 offset, signifying pos on segment
        ConnSegOrientaion m_segOrientation = ConnSegOrientaion::horizontal;
    };
} // namespace Bess::Canvas
