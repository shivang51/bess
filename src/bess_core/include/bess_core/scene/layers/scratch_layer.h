#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_layer.h"
namespace Bess::Canvas {
    class BESS_API ScratchLayer : public ISceneLayer {
      public:
        ScratchLayer() = default;
        ~ScratchLayer() override = default;

        void init(SceneLifecycleContext &ctx) override;
        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
    };

} // namespace Bess::Canvas
