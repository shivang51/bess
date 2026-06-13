#pragma once

#include "scene_layer.h"

namespace Bess::Canvas {
    class OverlayLayer : public ISceneLayer {
      public:
        void update(TimeMs ts, SceneContext &ctx) override;
        void draw(SceneContext &ctx) override;

        std::string getName() const override { return "OverlayLayer"; }

      private:
        void drawGhostConnection(SceneDrawContext &drawCtx,
                                 SceneContext &ctx) const;
        void drawSelectionBox(SceneDrawContext &drawCtx,
                              SceneContext &ctx) const;
    };
} // namespace Bess::Canvas
