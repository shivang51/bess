#pragma once

#include "common/bess_api.h"

#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas {

    // UI viewport bounds in window coordinates. Scene input events are formed
    // from this so layers can work in scene coordinates.
    enum class SceneMode : uint8_t {
        general,
    };

    enum class SceneDrawMode : uint8_t {
        none,
        connection,
    };

    struct BESS_API SelBoxContext {
        glm::vec2 start{0.f};
        glm::vec2 end{0.f};
        bool draw = false;
        bool queueSelInNextFrame = false;
        bool queueForSel = false;
    };

    struct BESS_API PickingReadbackRequest {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        bool active = false;
    };

    struct BESS_API SceneInputState {
        glm::vec2 mousePos{0.f};
        glm::vec2 dMousePos{0.f};
        bool isLeftMousePressed = false;
        bool isMiddleMousePressed = false;
        bool isDragging = false;
        bool isCtrlPressed = false;
        bool isShiftPressed = false;
        bool isAltPressed = false;
        SceneDrawMode drawMode = SceneDrawMode::none;
        SelBoxContext selectionBox;
        PickingReadbackRequest pickingReadbackRequest;

        void reset() {
            mousePos = {0.f, 0.f};
            dMousePos = {0.f, 0.f};
            isLeftMousePressed = false;
            isMiddleMousePressed = false;
            isDragging = false;
            isCtrlPressed = false;
            isShiftPressed = false;
            isAltPressed = false;
            drawMode = SceneDrawMode::none;
            selectionBox = {};
            pickingReadbackRequest = {};
        }
    };
} // namespace Bess::Canvas
