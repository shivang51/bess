#pragma once

#include "scene_layer.h"
#include <functional>
#include <vector>

namespace Bess::Canvas {
    class ScreenSpaceOverlayLayer : public ISceneLayer {
      public:
        using DrawCallback =
            std::function<void(SceneDrawContext &, SceneRenderContext &)>;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
        void reset(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;

        void addDrawCallback(DrawCallback callback);
        void clearDrawCallbacks();

        std::string getName() const override {
            return "ScreenSpaceOverlayLayer";
        }

      private:
        std::vector<DrawCallback> m_drawCallbacks;
    };
} // namespace Bess::Canvas
