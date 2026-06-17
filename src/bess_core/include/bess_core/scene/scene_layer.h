#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "camera.h"
#include "common/types.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_types.h"

namespace Bess::Canvas {

    enum class EventResult : uint8_t {
        Ignored,
        Handled,
        Consumed,
    };

    struct SceneLayerContext {
        SceneState *sceneState = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        const ViewportTransform *viewportTransform = nullptr;
        SceneInputState *inputState = nullptr;
        const PickingId *pickingId = nullptr;
    };

    struct SceneEventContext : SceneLayerContext {
        size_t viewportId;
        bool isSchematicMode;
    };

    struct SceneUpdateContext : SceneLayerContext {};

    struct SceneVpUpdateContext : SceneLayerContext {};

    struct SceneRenderContext : SceneLayerContext {
        size_t viewportId;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
        bool *isSchematicMode;
        bool isInFocusedViewport = true;
    };

    struct SceneLifecycleContext : SceneRenderContext {};

    class ISceneLayer {
      public:
        virtual ~ISceneLayer() = default;

        virtual EventResult handleEvent(SceneEvent &evt,
                                        SceneEventContext &ctx) {
            return EventResult::Ignored;
        }

        virtual bool shouldReceiveConsumedEvent(const SceneEvent &evt) const {
            return false;
        }

        virtual void update(TimeMs ts, SceneUpdateContext &ctx) = 0;

        virtual void draw(SceneRenderContext &ctx) = 0;

        virtual void viewportUpdate(TimeMs ts, SceneVpUpdateContext &ctx) {
        }

        virtual void init(SceneLifecycleContext &ctx) {
        }

        virtual void reset(SceneLifecycleContext &ctx) {
        }

        virtual void destroy(SceneLifecycleContext &ctx) {
        }

        virtual std::string getName() const {
            return "ISceneLayer";
        }
    };
} // namespace Bess::Canvas
