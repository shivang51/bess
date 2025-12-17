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

        virtual void onMouseButton(const Events::MouseButtonEvent &e) {}

        friend Derived;

      private:
        MouseBehaviour() = default;
    };
} // namespace Bess::Canvas
