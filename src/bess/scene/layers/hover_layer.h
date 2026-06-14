#pragma once

#include "common/types.h"
#include "scene_event.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    class HoverLayer : public ISceneLayer {
      public:
        EventResult handleEvent(SceneEvent &evt,
                                SceneEventContext &ctx) override;
        bool shouldReceiveConsumedEvent(const SceneEvent &evt) const override;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

        void reset(SceneLifecycleContext &ctx) override;
        void destroy(SceneLifecycleContext &ctx) override;

        std::string getName() const override {
            return "HoverLayer";
        }

      private:
        EventResult handleMouseMove(SceneEvent &evt, SceneEventContext &ctx);
        void clearHover(SceneState &state, const glm::vec2 &mousePos);

      private:
        PickingId m_pickingId = PickingId::invalid();
    };
} // namespace Bess::Canvas
