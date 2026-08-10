#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_layer.h"
#include "common/types.h"

namespace Bess::Canvas {
    class BESS_API SceneWidgetsLayer : public ISceneLayer {
      public:
        EventResult handleEvent(SceneEvent &evt,
                                SceneEventContext &ctx) override;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

        void reset(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;

        std::string getName() const override {
            return "SceneWidgetsLayer";
        }

      private:
        EventResult handleMouseMove(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseButton(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseWheel(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleKey(SceneEvent &evt, SceneEventContext &ctx);

      private:
        PickingId m_hoveredWidget = PickingId::invalid();
    };
} // namespace Bess::Canvas
