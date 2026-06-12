#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "camera.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include "scene_event.h"
#include "scene_state/scene_state.h"

namespace Bess::Canvas {

    struct SceneContext {
        SceneState *sceneState = nullptr;
        std::shared_ptr<Camera> camera = nullptr;
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
    };

    class ISceneLayer {
      public:
        virtual ~ISceneLayer() = default;

        virtual bool handleEvent(SceneEvent &evt, SceneContext &ctx) {
            return false;
        }

        virtual void update(TimeMs ts, SceneContext &ctx) = 0;
        virtual void draw(SceneContext &ctx) = 0;

        virtual void init(SceneContext &ctx) {}
        virtual void reset(SceneContext &ctx) {}
        virtual void destroy(SceneContext &ctx) {}

        virtual std::string getName() const { return "ISceneLayer"; }
    };
} // namespace Bess::Canvas
