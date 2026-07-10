#pragma once

#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_layer.h"
#include "common/types.h"

namespace Bess::Canvas {
    class UIComponentsLayer : public ISceneLayer {
      public:
        EventResult handleEvent(SceneEvent &evt,
                                SceneEventContext &ctx) override;
        bool shouldReceiveConsumedEvent(const SceneEvent &evt) const override;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

        void reset(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;

        std::string getName() const override {
            return "UIComponentsLayer";
        }

      private:
        EventResult handleMouseMove(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseButton(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseWheel(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleKey(SceneEvent &evt, SceneEventContext &ctx);

        void clearHover(SceneLayerContext &ctx, const glm::vec2 &mousePos);

      private:
        UUID m_hoveredComponent = UUID::null;
        UUID m_pressedComponent = UUID::null;
        PickingId m_hoveredPickingId = PickingId::invalid();
        PickingId m_pressedPickingId = PickingId::invalid();
    };
} // namespace Bess::Canvas
