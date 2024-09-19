#pragma once
#include "glm.hpp"

namespace Bess::UI::Types {
    enum class DrawMode { none = 0,
                          connection,
                          selectionBox };

    struct DragData {
        glm::vec2 dragOffset;
        bool isDragging;
        glm::vec2 vpMousePos;
    };

} // namespace Bess::UI::Types
