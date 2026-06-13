#pragma once

#include "scene/scene_events.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    class InteractionLayer : public ISceneLayer {
      public:
        EventResult handleEvent(SceneEvent &evt, SceneContext &ctx) override;

        void update(TimeMs ts, SceneContext &ctx) override;
        void draw(SceneContext &ctx) override;

        std::string getName() const override { return "InteractionLayer"; }

      private:
        EventResult handleMouseMove(SceneEvent &evt, SceneContext &ctx);
        EventResult handleMouseButton(SceneEvent &evt, SceneContext &ctx);
        EventResult handleMouseWheel(SceneEvent &evt, SceneContext &ctx);
        EventResult handleLeftMouseButton(SceneEvent &evt, SceneContext &ctx,
                                          bool isPressed);
        EventResult handleMiddleMouseButton(SceneEvent &evt, SceneContext &ctx,
                                            bool isPressed);
        void queueMouseButtonEvent(SceneEvent &evt, SceneContext &ctx,
                                   Events::MouseButton button,
                                   Events::MouseClickAction action) const;

        bool isCursorInViewport(SceneContext &ctx) const;
        void endActiveDrag(SceneContext &ctx) const;
    };
} // namespace Bess::Canvas
