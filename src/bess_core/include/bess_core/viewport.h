#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "ext/vector_float2.hpp"

namespace Bess::Core::Viewport {
    struct BESS_API ViewportTransfrom {
        glm::vec2 pos{0.f};
        glm::vec2 size{0.f};
    };

    enum class ViewportMode : uint8_t { normal, schematic };

    enum class ViewportDrawMode : uint8_t { none, connection };

    struct BESS_API ViewportConnDrawContext {
        UUID startCompId = UUID::null;

        void reset() {
            startCompId = UUID::null;
        }
    };

    struct BESS_API ViewportInputContext {
        glm::vec2 mousePos{0.f};
        glm::vec2 dMousePos{0.f};
        bool isLeftMousePressed = false;
        bool isMiddleMousePressed = false;
        bool isDragging = false;
        bool isCtrlPressed = false;
        bool isShiftPressed = false;
        bool isAltPressed = false;

        void reset() {
            mousePos = {0.f, 0.f};
            dMousePos = {0.f, 0.f};
            isLeftMousePressed = false;
            isMiddleMousePressed = false;
            isDragging = false;
            isCtrlPressed = false;
            isShiftPressed = false;
            isAltPressed = false;
        }
    };

    struct BESS_API PickingReadbackRequest {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        bool active = false;

        void reset() {
            x = 0;
            y = 0;
            width = 0;
            height = 0;
            active = false;
        }
    };

    struct BESS_API SelBoxContext {
        glm::vec2 start{0.f};
        glm::vec2 end{0.f};
        bool draw = false;
        bool queueSelInNextFrame = false;
        bool queueForSel = false;

        void reset() {
            start = {0.f, 0.f};
            end = {0.f, 0.f};
            draw = false;
            queueSelInNextFrame = false;
            queueForSel = false;
        }
    };

    struct BESS_API ViewportContext {
        ViewportTransfrom transform;
        ViewportMode mode;
        ViewportDrawMode drawMode;
        ViewportConnDrawContext connDrawCtx;
        ViewportInputContext inputCtx;
        PickingReadbackRequest pickingReadbackRequest;
        SelBoxContext selBoxCtx;
        size_t viewportId = 0;

        void reset() {
            transform = {};
            mode = ViewportMode::normal;
            drawMode = ViewportDrawMode::none;
            connDrawCtx.reset();
            inputCtx.reset();
            pickingReadbackRequest.reset();
            selBoxCtx.reset();
        }
    };

} // namespace Bess::Core::Viewport
