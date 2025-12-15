#pragma once

#include <glm.hpp>

namespace Bess::Canvas {
    template <typename Derived>
    class MouseBehaviour {
      public:
        virtual void onMouseHovered(const glm::vec2 &mousePos) {}
        virtual void onMouseEnter() {}
        virtual void onMouseLeave() {}

        virtual void onLeftMouseClicked(const glm::vec2 &mousePos) {}
        virtual void onLeftMouseReleased(const glm::vec2 &mousePos) {}
        virtual void onMiddleMouseClicked(const glm::vec2 &mousePos) {}
        virtual void onMiddleMouseReleased(const glm::vec2 &mousePos) {}
        virtual void onRightMouseClicked(const glm::vec2 &mousePos) {}
        virtual void onRightMouseReleased(const glm::vec2 &mousePos) {}

        friend Derived;

      private:
        MouseBehaviour() = default;
    };
} // namespace Bess::Canvas
