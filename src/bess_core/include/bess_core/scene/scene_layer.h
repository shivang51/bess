#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/viewport.h"
#include "camera.h"
#include "common/types.h"

namespace Bess::Canvas {

    namespace SceneWidgets {
        struct SceneWidgetsState;
    } // namespace SceneWidgets

    enum class EventResult : uint8_t {
        Ignored,
        Handled,
        Consumed,
    };

    struct BESS_API SceneLayerContext {
        SceneState *sceneState = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        std::shared_ptr<Core::Viewport::ViewportContext> viewportCtx = nullptr;
        SceneWidgets::SceneWidgetsState *sceneWidgetsState = nullptr;
    };

    struct BESS_API SceneEventContext : SceneLayerContext {};

    struct BESS_API SceneUpdateContext : SceneLayerContext {};

    struct BESS_API SceneVpUpdateContext : SceneLayerContext {
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
    };

    struct BESS_API SceneRenderContext : SceneLayerContext {
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
        std::shared_ptr<SimEngine::SimulationEngine> simEngine = nullptr;
    };

    struct BESS_API SceneLifecycleContext : SceneRenderContext {};

    class BESS_API ISceneLayer {
      public:
        virtual ~ISceneLayer() = default;

        virtual EventResult handleEvent(SceneEvent & /*evt*/,
                                        SceneEventContext & /*ctx*/) {
            return EventResult::Ignored;
        }

        virtual bool
        shouldReceiveConsumedEvent(const SceneEvent & /*evt*/) const {
            return false;
        }

        virtual void update(TimeMs ts, SceneUpdateContext &ctx) = 0;

        virtual void draw(SceneRenderContext &ctx) = 0;

        virtual void viewportUpdate(TimeMs /*ts*/,
                                    SceneVpUpdateContext & /*ctx*/) {
        }

        virtual void init(SceneLifecycleContext & /*ctx*/) {
        }

        virtual void reset(SceneLifecycleContext & /*ctx*/) {
        }

        virtual void destroy(SceneLifecycleContext & /*ctx*/) {
        }

        virtual std::string getName() const {
            return "ISceneLayer";
        }
    };
} // namespace Bess::Canvas
