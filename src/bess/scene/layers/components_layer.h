#pragma once

#include "common/types.h"
#include "scene_event.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    class ComponentsLayer : public ISceneLayer {
      public:
        ComponentsLayer() = default;
        ~ComponentsLayer() override = default;

        EventResult handleEvent(SceneEvent &evt, SceneContext &ctx) override;

        void init(SceneContext &ctx) override;
        void destroy(SceneContext &ctx) override;

        void update(TimeMs ts, SceneContext &ctx) override;
        void draw(SceneContext &ctx) override;

      private:
        void handleMouseMove(SceneEvent &evt, SceneContext &ctx);

      private:
        PickingId m_pickingId = PickingId::invalid();
    };
} // namespace Bess::Canvas
