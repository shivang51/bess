#pragma once
#include "glm.hpp"

namespace Bess::UI::Types {
    enum class DrawMode { none = 0,
                          connection,
                          selectionBox };

    struct DragData {
        glm::vec2 dragOffset = {0, 0};
        bool isDragging = false;
        glm::vec2 vpMousePos = {0, 0};
    };

} // namespace Bess::UI::Types
