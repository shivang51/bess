#pragma once

namespace Bess::Canvas {
    template <typename Derived>
    class DragBehaviour {
      public:
        void onMouseDragged(const glm::vec2 &mousePos) {
            if (!m_isDragging) {
                onDragBegin(mousePos);
            }
            auto &self = static_cast<Derived &>(*this);
            const auto newPos = mousePos + m_dragOffset;
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

        void onDragBegin(const glm::vec2 &mousePos) {
            auto &self = static_cast<Derived &>(*this);
            m_dragOffset = glm::vec2(self.getTransform().position) - mousePos;
            m_isDragging = true;
        }

        bool m_isDragging = false;
        glm::vec2 m_dragOffset = {0.f, 0.f};
        friend Derived;
    };
} // namespace Bess::Canvas
