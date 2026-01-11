#pragma once

#include "events/scene_events.h"
namespace Bess::Canvas {
    constexpr float snapAmount = 5.f;
    template <typename Derived>
    class DragBehaviour {
      public:
        virtual void onMouseDragged(const Events::MouseDraggedEvent &e) {
            if (!m_isDragging) {
                onMouseDragBegin(e);
            }
            auto &self = static_cast<Derived &>(*this);
            auto newPos = e.mousePos + m_dragOffset;
            newPos = glm::round(newPos / snapAmount) * snapAmount;
            self.setPosition(glm::vec3(newPos, self.getTransform().position.z));
        }

        void onMouseDragEnd() {
            m_isDragging = false;
        }

      private:
        DragBehaviour() {
            auto &self = static_cast<Derived &>(*this);
            self.setIsDraggable(true);
        }

      protected:
        void initDragBehaviour() {
            auto &self = static_cast<Derived &>(*this);
            self.setIsDraggable(true);
        }

        virtual void onMouseDragBegin(const Events::MouseDraggedEvent &e) {
            auto &self = static_cast<Derived &>(*this);
            m_dragOffset = glm::vec2(self.getTransform().position) - e.mousePos;
            m_isDragging = true;
        }

        bool m_isDragging = false;
        glm::vec2 m_dragOffset = {0.f, 0.f};
        friend Derived;
    };
} // namespace Bess::Canvas
