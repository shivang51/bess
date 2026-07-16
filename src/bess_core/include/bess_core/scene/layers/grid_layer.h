#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_layer.h"

namespace Bess::Canvas {
    class BESS_API GridLayer : public ISceneLayer {
      public:
        GridLayer() = default;
        ~GridLayer() override = default;

        void init(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

      private:
        Core::Renderer::CustomQuadShaderHandle m_gridShader = 0;
    };
} // namespace Bess::Canvas
