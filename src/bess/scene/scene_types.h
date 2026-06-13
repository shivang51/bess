#pragma once

#include <cstdint>
#include <glm.hpp>

namespace Bess::Canvas {

    // UI viewport bounds in window coordinates. Scene input events are formed
    // from this so layers can work in scene coordinates.
    struct ViewportTransform {
        glm::vec2 pos{0.f};
        glm::vec2 size{0.f};
    };

    enum class SceneMode : uint8_t {
        general,
    };

    enum class SceneDrawMode : uint8_t {
        none,
        connection,
    };

    enum class SceneCursor : uint8_t {
        inherit,
        normal,
        pointer,
        move,
    };

    struct SelBoxContext {
        glm::vec2 start{0.f};
        glm::vec2 end{0.f};
        bool draw = false;
        bool queueSelInNextFrame = false;
        bool queueForSel = false;
    };

    struct PickingReadbackRequest {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        bool active = false;
    };

    struct SceneInputState {
        glm::vec2 mousePos{0.f};
        glm::vec2 dMousePos{0.f};
        bool isLeftMousePressed = false;
        bool isMiddleMousePressed = false;
        bool isDragging = false;
        bool isCtrlPressed = false;
        bool isShiftPressed = false;
        bool isAltPressed = false;
        SceneDrawMode drawMode = SceneDrawMode::none;
        SceneCursor cursor = SceneCursor::inherit;
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
            cursor = SceneCursor::inherit;
            selectionBox = {};
            pickingReadbackRequest = {};
        }
    };
} // namespace Bess::Canvas
