#include "screen_space_overlay_layer.h"
#include "scene/widgets/scene_widgets.h"

#include <utility>

namespace Bess::Canvas {
    void ScreenSpaceOverlayLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScreenSpaceOverlayLayer::draw(SceneRenderContext &ctx) {
        if (!ctx.sceneState || !ctx.renderer || !ctx.camera) {
            return;
        }

        SceneDrawContext drawCtx{
            ctx.sceneState,
            ctx.renderer,
            ctx.camera,
            Core::Renderer::RenderTransformMode::Screen,
        };

        for (auto &callback : m_drawCallbacks) {
            if (callback) {
                callback(drawCtx, ctx);
            }
        }

        static bool show = true;

        if (show) {
            ctx.renderer->drawFont(
                "Hello World",
                {
                    .position = {0, 0},
                    .fontSize = 32.f,
                    .color = Bess::Core::Renderer::Colors::white,
                    .zIndex = 1000,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });
        }

        if (SceneWidgets::button(PickingId{2, 0},
                                 "Toggle Overlay",
                                 {10.f, 50.f, 1000},
                                 drawCtx,
                                 {
                                     .textSize = 32.f,
                                 })) {
            show = !show;
        }
    }

    void ScreenSpaceOverlayLayer::reset(SceneLifecycleContext &ctx) {
    }

    void ScreenSpaceOverlayLayer::destroy(SceneLifecycleContext &ctx) {
        clearDrawCallbacks();
    }

    void ScreenSpaceOverlayLayer::addDrawCallback(DrawCallback callback) {
        m_drawCallbacks.push_back(std::move(callback));
    }

    void ScreenSpaceOverlayLayer::clearDrawCallbacks() {
        m_drawCallbacks.clear();
    }
} // namespace Bess::Canvas
