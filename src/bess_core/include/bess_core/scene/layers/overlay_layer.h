#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_layer.h"

namespace Bess::Canvas {
    class BESS_API OverlayLayer : public ISceneLayer {
      public:
        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

        std::string getName() const override {
            return "OverlayLayer";
        }

      private:
        void drawGhostConnection(SceneDrawContext &drawCtx,
                                 SceneRenderContext &ctx) const;
        void drawSelectionBox(SceneDrawContext &drawCtx,
                              SceneRenderContext &ctx) const;
    };
} // namespace Bess::Canvas
