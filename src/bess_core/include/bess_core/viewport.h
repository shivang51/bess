#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include <memory>

namespace Bess::Canvas::SceneWidgets {
    struct ViewportSceneWidgetsState;
} // namespace Bess::Canvas::SceneWidgets

namespace Bess::Core::Viewport {
    struct BESS_API ViewportTransform {
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

    enum class SceneCursor : uint8_t {
        inherit,
        normal,
        pointer,
        move,
        text,
    };

    struct BESS_API CursorRequest {
        SceneCursor cursor = SceneCursor::inherit;
        uint8_t priority = 0;

        void reset() {
            cursor = SceneCursor::inherit;
            priority = 0;
        }
    };

    struct BESS_API ViewportInputContext {
        PickingId pickingId = PickingId::invalid();
        glm::vec2 mousePos{0.f};
        glm::vec2 dMousePos{0.f};
        bool isLeftMousePressed = false;
        bool isMiddleMousePressed = false;
        bool isDragging = false;
        bool isCtrlPressed = false;
        bool isShiftPressed = false;
        bool isAltPressed = false;
        CursorRequest cursorRequest;
        SceneCursor lastAppliedCursor = SceneCursor::inherit;

        void requestCursor(SceneCursor cursor, uint8_t priority = 0) {
            if (cursor == SceneCursor::inherit) {
                return;
            }

            if (priority >= cursorRequest.priority) {
                cursorRequest.cursor = cursor;
                cursorRequest.priority = priority;
            }
        }

        void resetCursorRequest() {
            cursorRequest.reset();
        }

        void reset() {
            mousePos = {0.f, 0.f};
            dMousePos = {0.f, 0.f};
            isLeftMousePressed = false;
            isMiddleMousePressed = false;
            isDragging = false;
            isCtrlPressed = false;
            isShiftPressed = false;
            isAltPressed = false;
            pickingId = PickingId::invalid();
            cursorRequest.reset();
            lastAppliedCursor = SceneCursor::inherit;
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
        ViewportContext();
        ~ViewportContext();

        ViewportContext(const ViewportContext &) = delete;
        ViewportContext &operator=(const ViewportContext &) = delete;
        ViewportContext(ViewportContext &&) noexcept;
        ViewportContext &operator=(ViewportContext &&) noexcept;

        ViewportTransform transform;
        ViewportMode mode = ViewportMode::normal;
        ViewportDrawMode drawMode = ViewportDrawMode::none;
        ViewportConnDrawContext connDrawCtx;
        ViewportInputContext inputCtx;
        PickingReadbackRequest pickingReadbackRequest;
        SelBoxContext selBoxCtx;
        std::unique_ptr<Bess::Canvas::SceneWidgets::ViewportSceneWidgetsState>
            sceneWidgetsState;
        UUID updateSceneId = UUID::null;
        size_t viewportId = 0;
        bool isFocused = false;
        bool isResized = false;

        bool isSchematicMode() const {
            return mode == ViewportMode::schematic;
        }

        void toggleSchematicMode() {
            mode = isSchematicMode() ? ViewportMode::normal
                                     : ViewportMode::schematic;
        }

        // Does not reset transform or viewportId. Clears transient input,
        // selection, and scene-widget state owned by this viewport.
        void reset();
    };

} // namespace Bess::Core::Viewport
