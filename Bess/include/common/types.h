#pragma once
#include "glm.hpp"

namespace Bess::UI::Types {
    enum class DrawMode { none = 0,
                          connection };

    struct DragData {
        glm::vec2 dragOffset;
        bool isDragging;
    };

} // namespace Bess::UI::Types
