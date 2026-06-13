#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "camera.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include "scene_event.h"
#include "scene_state/scene_state.h"
#include "scene_types.h"

namespace Bess::Canvas {

    enum class EventResult : uint8_t {
        Ignored,
        Handled,
        Consumed,
    };

    struct SceneContext {
        SceneState *sceneState = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
        ViewportTransform *viewportTransform = nullptr;
        SelBoxContext *selBoxContext = nullptr;
        PickingReadbackRequest *pickingReadbackRequest = nullptr;
        PickingId *pickingId = nullptr;
        glm::vec2 *mousePos = nullptr;
        glm::vec2 *dMousePos = nullptr;
        bool *isLeftMousePressed = nullptr;
        bool *isMiddleMousePressed = nullptr;
        bool *isDragging = nullptr;
        SceneDrawMode *drawMode = nullptr;
    };

    class ISceneLayer {
      public:
        virtual ~ISceneLayer() = default;

        virtual EventResult handleEvent(SceneEvent &evt, SceneContext &ctx) {
            return EventResult::Ignored;
        }

        virtual void update(TimeMs ts, SceneContext &ctx) = 0;
        virtual void draw(SceneContext &ctx) = 0;

        virtual void init(SceneContext &ctx) {}
        virtual void reset(SceneContext &ctx) {}
        virtual void destroy(SceneContext &ctx) {}

        virtual std::string getName() const { return "ISceneLayer"; }
    };
} // namespace Bess::Canvas
