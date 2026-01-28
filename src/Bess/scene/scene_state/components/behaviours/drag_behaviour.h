#pragma once

#include "events/scene_events.h"
namespace Bess::Canvas {
    constexpr float SNAP_ANOUNT = 2.f;
    template <typename Derived>
    class DragBehaviour {
      public:
        DragBehaviour() {
            initDragBehaviour();
        }

        virtual void onMouseDragged(const Events::MouseDraggedEvent &e) {
            if (!m_isDragging) {
                onMouseDragBegin(e);
            }
            auto &self = static_cast<Derived &>(*this);
            auto newPos = e.mousePos + m_dragOffset;
            newPos = glm::round(newPos / SNAP_ANOUNT) * SNAP_ANOUNT;
            self.setPosition(glm::vec3(newPos, self.getTransform().position.z));
        }

        void onMouseDragEnd() {
            m_isDragging = false;
        }

      protected:
        void initDragBehaviour() {
            auto &self = static_cast<Derived &>(*this);
            self.setIsDraggable(true);
        }

        virtual void onMouseDragBegin(const Events::MouseDraggedEvent &e) {
            const auto &self = static_cast<const Derived &>(*this);
            m_dragOffset = glm::vec2(self.getAbsolutePosition(*e.sceneState)) - e.mousePos;
            m_isDragging = true;
        }

        bool m_isDragging = false;
        glm::vec2 m_dragOffset = {0.f, 0.f};
        friend Derived;
    };
} // namespace Bess::Canvas
