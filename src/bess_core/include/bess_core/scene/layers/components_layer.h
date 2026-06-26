#pragma once

#include "bess_core/scene/scene_layer.h"
#include "common/types.h"

namespace Bess::Canvas {
    class ComponentsLayer : public ISceneLayer {
      public:
        ComponentsLayer() = default;
        ~ComponentsLayer() override = default;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
        void viewportUpdate(TimeMs dt, SceneVpUpdateContext &ctx) override;
    };
} // namespace Bess::Canvas
