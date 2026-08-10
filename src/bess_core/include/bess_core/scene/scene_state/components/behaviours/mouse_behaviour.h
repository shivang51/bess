#pragma once

#include "bess_core/scene/scene_events.h"
#include <glm.hpp>

namespace Bess::Canvas {
    template <typename Derived> class MouseBehaviour {
      public:
        [[nodiscard]] virtual bool
        onMouseHovered(const Events::MouseHoveredEvent &e) {
            return false;
        }

        [[nodiscard]] virtual bool
        onMouseEnter(const Events::MouseEnterEvent &e) {
            return false;
        }

        [[nodiscard]] virtual bool
        onMouseLeave(const Events::MouseLeaveEvent &e) {
            return false;
        }

        [[nodiscard]] virtual bool
        onMouseButton(const Events::MouseButtonEvent &e) {
            return false;
        }

        [[nodiscard]] virtual bool
        onMouseWheel(const Events::MouseWheelEvent &e) {
            return false;
        }

        [[nodiscard]] virtual bool
        onMouseMove(const Events::MouseMoveEvent &e) {
            return false;
        }

        friend Derived;

      private:
        MouseBehaviour() = default;
    };
} // namespace Bess::Canvas
