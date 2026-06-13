#pragma once

#include "scene/scene_events.h"
#include "scene_layer.h"

namespace Bess::Canvas {
    class InteractionLayer : public ISceneLayer {
      public:
        EventResult handleEvent(SceneEvent &evt,
                                SceneEventContext &ctx) override;

        void update(TimeMs ts, SceneUpdateContext &ctx) override;
        void draw(SceneRenderContext &ctx) override;

        std::string getName() const override {
            return "InteractionLayer";
        }

      private:
        EventResult handleMouseMove(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseButton(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleMouseWheel(SceneEvent &evt, SceneEventContext &ctx);
        EventResult handleLeftMouseButton(SceneEvent &evt,
                                          SceneEventContext &ctx,
                                          bool isPressed);
        EventResult handleMiddleMouseButton(SceneEvent &evt,
                                            SceneEventContext &ctx,
                                            bool isPressed);
        void queueMouseButtonEvent(SceneEvent &evt,
                                   SceneEventContext &ctx,
                                   Events::MouseButton button,
                                   Events::MouseClickAction action) const;

        bool isCursorInViewport(SceneEventContext &ctx) const;
        void endActiveDrag(SceneEventContext &ctx) const;
    };
} // namespace Bess::Canvas
