#pragma once

#include "common/types.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    class ComponentsLayer : public ISceneLayer {
      public:
        ComponentsLayer() = default;
        ~ComponentsLayer() override = default;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;
    };
} // namespace Bess::Canvas
