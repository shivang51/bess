#pragma once

#include "bess_core/scene/scene_layer.h"
namespace Bess::Canvas {
    class ScratchLayer : public ISceneLayer {
      public:
        ScratchLayer() = default;
        ~ScratchLayer() override = default;

        void init(SceneLifecycleContext &ctx) override;
        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

      private:
        float m_radius = 0.f;
    };

} // namespace Bess::Canvas
