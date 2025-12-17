#pragma once

#include "events/scene_events.h"
#include <glm.hpp>

namespace Bess::Canvas {
    template <typename Derived>
    class MouseBehaviour {
      public:
        virtual void onMouseHovered(const Events::MouseHoveredEvent &e) {}
        virtual void onMouseEnter(const Events::MouseEnterEvent &e) {}
        virtual void onMouseLeave(const Events::MouseLeaveEvent &e) {}

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
