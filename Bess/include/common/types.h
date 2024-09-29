#pragma once
#include "glm.hpp"

namespace Bess::Types {
    struct DragData {
        glm::vec2 dragOffset = {0, 0};
        bool isDragging = false;
        glm::vec2 vpMousePos = {0, 0};
        glm::vec3 orinalEntPos = {0, 0, 0};
    };
} // namespace Bess::Types

namespace Bess::UI::Types {
    enum class DrawMode { none = 0,
                          connection,
                          selectionBox };

} // namespace Bess::UI::Types
