#include "screen_space_overlay_layer.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "scene/widgets/scene_widgets.h"
#include "settings/viewport_theme.h"

#include <utility>

namespace Bess::Canvas {
    namespace {
        constexpr float padding = 8.f;
        constexpr float fontSize = 16.f;

        void drawCameraPos(SceneDrawContext &drawCtx,
                           SceneRenderContext &ctx,
                           const glm::vec2 &bottomRight) {
            static constexpr glm::vec2 textSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    "X: -000.00Y: -000.00", {.fontSize = fontSize});

            const glm::vec2 mouseWorldPos =
                ctx.camera->toWorldPos(ctx.inputState->mousePos);

            // aligns the text to the right, padding the left side with spaces
            const std::string xText =
                std::format("X: {:>9.2f}", mouseWorldPos.x);
            const std::string yText =
                std::format("    Y: {:>9.2f}", mouseWorldPos.y);

            const auto textOffY = ctx.renderer->textCenterOffsetY(
                xText,
                {.fontSize = fontSize,
                 .transformMode = Core::Renderer::RenderTransformMode::Screen});

            const auto quadSize =
                textSize + glm::vec2{padding * 2.f, padding * 2.f};
            const auto quadPos = bottomRight - (quadSize / 2.f);
            const auto textPos =
                quadPos + glm::vec2{(-quadSize.x / 2.f) + padding, textOffY};

            static constexpr auto pickingId = PickingId::forWidget(0);

            if (SceneWidgets::button(
                    pickingId,
                    "",
                    {quadPos.x, quadPos.y, 1000},
                    drawCtx,
                    {
                        .textSize = fontSize,
                        .buttonSize = quadSize,
                        .padding = glm::vec2{padding},
                        .borderThickness = glm::vec4(0.f),
                        .borderRadius = glm::vec4(8.f),
                        .shadow = Core::Renderer::ShadowProps{.enabled = true},
                        .backgroundColor =
                            ViewportTheme::sceneWidgetsColors.surface.withAlpha(
                                0.5f),
                    })) {
                ctx.camera->focusAtPoint({0.f, 0.f}, false);
            }

            ctx.renderer->drawFont(
                xText,
                {
                    .position = {textPos.x, textPos.y},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.text,
                    .zIndex = 1000.1,
                    .id = pickingId,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            ctx.renderer->drawFont(
                "|",
                {
                    .position = {quadPos.x, textPos.y},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.textMuted,
                    .zIndex = 1000.1,
                    .id = pickingId,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            ctx.renderer->drawFont(
                yText,
                {
                    .position = {quadPos.x, textPos.y},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.text,
                    .zIndex = 1000.1,
                    .id = PickingId{0, 0},
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });
        }

        float drawCameraZoom(SceneDrawContext &drawCtx,
                             SceneRenderContext &ctx,
                             const glm::vec2 &bottomRight) {
            constexpr glm::vec2 textSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    "Zoom: 0.00x", {.fontSize = fontSize});

            const std::string zoomText =
                std::format("Zoom: {:.2f}x", ctx.camera->getZoom());

            const auto textOffY = ctx.renderer->textCenterOffsetY(
                zoomText,
                {.fontSize = fontSize,
                 .transformMode = Core::Renderer::RenderTransformMode::Screen});

            const auto quadSize =
                textSize + glm::vec2{padding * 2.f, padding * 2.f};
            const auto quadPos =
                bottomRight +
                glm::vec2{(-quadSize.x / 2.f), -((textSize.y / 2.f) + padding)};

            Core::Renderer::QuadProps quad{
                .position = quadPos,
                .size = quadSize,
                .zIndex = 1000,
                .color =
                    ViewportTheme::sceneWidgetsColors.surface.withAlpha(0.5f),
                .transformMode = Core::Renderer::RenderTransformMode::Screen,
                .radius = glm::vec4(8.f),
                .shadow = {.enabled = true},
            };

            ctx.renderer->drawQuad(quad);

            const auto textPos =
                quadPos + glm::vec2{(-quadSize.x / 2.f) + padding, textOffY};

            ctx.renderer->drawFont(
                zoomText,
                {
                    .position = {textPos.x, textPos.y},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.text,
                    .zIndex = 1000.1,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            return quadSize.x + padding;
        }
    } // namespace

    void ScreenSpaceOverlayLayer::draw(SceneRenderContext &ctx) {
        if (!ctx.sceneState || !ctx.renderer || !ctx.camera) {
            return;
        }

        SceneDrawContext drawCtx{
            ctx.sceneState,
            ctx.renderer,
            ctx.camera,
            Core::Renderer::RenderTransformMode::Screen,
            ctx.viewportId,
        };

        for (auto &callback : m_drawCallbacks) {
            if (callback) {
                callback(drawCtx, ctx);
            }
        }

        auto bottomRight = (ctx.viewportTransform->size / 2.f) - padding;
        float xOffset = drawCameraZoom(drawCtx, ctx, bottomRight);
        bottomRight.x -= xOffset;
        drawCameraPos(drawCtx, ctx, bottomRight);
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

    void ScreenSpaceOverlayLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

} // namespace Bess::Canvas
