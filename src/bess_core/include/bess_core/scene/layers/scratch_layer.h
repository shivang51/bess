#pragma once

#include "bess_core/scene/scene_layer.h"
namespace Bess::Canvas {
    class ScratchLayer : public ISceneLayer {
      public:
        ScratchLayer() = default;
        ~ScratchLayer() override = default;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
    };

} // namespace Bess::Canvas
