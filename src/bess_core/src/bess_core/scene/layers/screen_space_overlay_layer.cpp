#include "bess_core/scene/layers/screen_space_overlay_layer.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/scene/scene_ui/ui_view.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/scene_driver.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/icons/MaterialIcons_Remapped.h"

#include <cstdint>
#include <utility>

namespace Icons = Bess::UI::Icons;

namespace Bess::Canvas {
    namespace {
        constexpr float padding = 6.f;
        constexpr float fontSize = 14.f;

        void drawCameraPos(SceneDrawContext &drawCtx,
                           SceneRenderContext &ctx,
                           const glm::vec2 &bottomRight) {
            static constexpr glm::vec2 textSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    "X: -000.00Y: -000.00", {.fontSize = fontSize});

            const glm::vec2 mouseWorldPos =
                ctx.camera->toWorldPos(ctx.viewportCtx->inputCtx.mousePos);

            // aligns the text to the right, padding the left side with spaces
            const std::string xText = std::format(
                "{} {:>9.2f}", Icons::FontAwesomeIcons::FA_X, mouseWorldPos.x);
            const std::string yText = std::format(
                "{} {:>9.2f}", Icons::FontAwesomeIcons::FA_Y, mouseWorldPos.y);

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
                    .position = {quadPos.x + 24.f, textPos.y},
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
                    "0.00x ZZ", {.fontSize = fontSize});

            const std::string zoomText =
                std::format("{:.2f}x", ctx.camera->getZoom());

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

            ctx.renderer->drawFont(
                std::format(" {}", Icons::CodIcons::ZOOM_IN),
                {
                    .position = {textPos.x + (textSize.x / 2.f) + 8.f,
                                 textPos.y + 2.f},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.text,
                    .zIndex = 1000.1,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            return quadSize.x + padding;
        }

        glm::vec2 drawSchematicToggle(SceneDrawContext &drawCtx,
                                      SceneRenderContext &ctx,
                                      const glm::vec2 &topLeft) {
            BESS_ASSERT(ctx.viewportCtx, "ViewportCtx is invalid");

            static constexpr std::string_view label = "Schematic View";
            static constexpr glm::vec2 textSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    label, {.fontSize = fontSize});
            static constexpr float toggleWidth = 28.f;
            static constexpr float gapX = 8.f;
            static constexpr glm::vec2 boxSize =
                textSize + glm::vec2{padding * 2.f, padding * 2.f} +
                glm::vec2{toggleWidth + gapX, 0.f};

            const auto textOffY = ctx.renderer->textCenterOffsetY(
                label,
                {
                    .fontSize = fontSize,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            const glm::vec2 boxPos = {
                topLeft.x + (boxSize.x / 2.f),
                topLeft.y + (boxSize.y / 2.f),
            };

            ctx.renderer->drawQuad({
                .position = boxPos,
                .size = boxSize,
                .zIndex = 1000,
                .color =
                    ViewportTheme::sceneWidgetsColors.surface.withAlpha(0.5f),
                .transformMode = Core::Renderer::RenderTransformMode::Screen,
                .radius = glm::vec4(8.f),
                .shadow = {.enabled = true},
            });

            const auto left = boxPos.x - (boxSize.x / 2.f) + padding;
            const auto yPos = boxPos.y;

            ctx.renderer->drawFont(
                label,
                {
                    .position = {left, yPos + textOffY},
                    .fontSize = fontSize,
                    .color = ViewportTheme::sceneWidgetsColors.text,
                    .zIndex = 1000.1,
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

            if (SceneWidgets::toggleButton(PickingId::forWidget(1),
                                           ctx.viewportCtx->isSchematicMode(),
                                           {
                                               left + textSize.x + gapX,
                                               yPos,
                                               1000.1,
                                           },
                                           {28.f, 16.f},
                                           drawCtx)) {
                ctx.viewportCtx->toggleSchematicMode();
            }

            return boxPos + glm::vec2{boxSize.x / 2.f, 0.f};
        }

        void drawSceneControls(SceneDrawContext &drawCtx,
                               SceneRenderContext &ctx,
                               const glm::vec2 &offset) {

            constexpr std::string_view rootLabel = "Root";
            constexpr glm::vec2 textSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    rootLabel, {.fontSize = fontSize});

            const float textOffY =
                ctx.renderer->textCenterOffsetY("Root",
                                                {
                                                    .fontSize = fontSize,
                                                });

            glm::vec2 cursor = offset + glm::vec2{padding + 16.f, 0.f};

            if (ctx.sceneState->getIsRootScene()) {
                ctx.renderer->drawFont(
                    rootLabel,
                    {
                        .position = {cursor.x, cursor.y + textOffY},
                        .fontSize = fontSize,
                        .color = ViewportTheme::sceneWidgetsColors.textMuted,
                        .zIndex = 1000.1,
                        .transformMode =
                            Core::Renderer::RenderTransformMode::Screen,
                    });

                return;
            }

            const auto &appCtx = GAppContext::getInstance();
            const auto &sceneDriver = appCtx.getSubSystem<ProjectContext>()
                                          ->getSubSystem<SceneDriver>();

            uint32_t btnId = 2;

            const auto drawSceneButton = [&](const SceneState *state,
                                             const SceneState *parentState) {
                BESS_ASSERT(state, "SceneState pointer is null");

                const auto sceneId = state->getSceneId();
                std::string sceneName = "Root";

                if (parentState) {
                    const auto moduleComp =
                        parentState->getComponentByUuid(state->getModuleId());
                    BESS_ASSERT(moduleComp,
                                "Module component not found for scene {}",
                                (uint64_t)state->getSceneId());
                    if (moduleComp) {
                        sceneName = moduleComp->getName();
                    }
                }

                const auto buttonSize = ctx.renderer->measureText(
                                            sceneName, {.fontSize = fontSize}) +
                                        glm::vec2{padding * 2.f, padding * 2.f};

                const auto buttonPos = glm::vec3(
                    cursor.x + (buttonSize.x / 2.f), cursor.y, 1000.1f);

                if (SceneWidgets::button(
                        PickingId::forWidget(btnId++),
                        sceneName,
                        buttonPos,
                        drawCtx,
                        {
                            .textSize = fontSize,
                            .buttonSize = buttonSize,
                            .padding = glm::vec2{padding},
                            .borderThickness = glm::vec4(0.f),
                            .borderRadius = glm::vec4(8.f),
                            .backgroundColor = ViewportTheme::sceneWidgetsColors
                                                   .surface.withAlpha(0.25f),
                        })) {
                    ctx.viewportCtx->updateSceneId = sceneId;
                }

                cursor.x += buttonSize.x + padding;

                ctx.renderer->drawFont(
                    Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT,
                    {
                        .position = {cursor.x, cursor.y + textOffY},
                        .fontSize = fontSize,
                        .color = ViewportTheme::sceneWidgetsColors.textMuted,
                        .zIndex = 1000.1,
                        .transformMode =
                            Core::Renderer::RenderTransformMode::Screen,
                    });

                cursor.x += padding * 2.f;
            };

            const std::function<void(const SceneState *, bool)> drawSceneBtns =
                [&](const SceneState *state, bool isLeaf) {
                    if (!state)
                        return;

                    const auto parentSceneId = state->getParentSceneId();
                    auto parentScene =
                        sceneDriver->getSceneWithId(parentSceneId);

                    drawSceneBtns(parentScene ? &parentScene->getState()
                                              : nullptr,
                                  false);

                    if (!isLeaf) {
                        drawSceneButton(state,
                                        parentScene ? &parentScene->getState()
                                                    : nullptr);

                        return;
                    }

                    ctx.renderer->drawFont(
                        "Leaf",
                        {
                            .position = {cursor.x, cursor.y + textOffY},
                            .fontSize = fontSize,
                            .color =
                                ViewportTheme::sceneWidgetsColors.textMuted,
                            .zIndex = 1000.1,
                            .transformMode =
                                Core::Renderer::RenderTransformMode::Screen,
                        });
                };

            drawSceneBtns(ctx.sceneState, true);
        }

        // Draws from the right side
        void drawSchematicViewControls(SceneDrawContext &drawCtx,
                                       SceneRenderContext &ctx,
                                       const glm::vec2 &offset) {
            constexpr glm::vec2 buttonSize =
                Core::Renderer::IRenderer2D::getTextRenderSize(
                    Icons::MaterialIcons::FLIP, {.fontSize = fontSize}) +
                glm::vec2{padding * 2.f, padding * 2.f};

            if (!ctx.viewportCtx->isSchematicMode()) {
                return;
            }

            auto selComps = ctx.sceneState->getSelectedComponents();

            if (selComps.empty()) {
                return;
            }

            auto selComp =
                ctx.sceneState->getComponentByUuid(selComps.begin()->first);

            if (!selComp ||
                selComp->getType() != SceneComponentType::simulation) {
                return;
            }
            auto &style = selComp->getStyle();
            const auto isFlipped = style.schematicStyle.flipSlotsX;

            glm::vec2 cursor =
                offset + glm::vec2{-buttonSize.x / 2.f, buttonSize.y / 2.f};
            if (Canvas::SceneWidgets::button(
                    PickingId::forWidget(100),
                    Icons::MaterialIcons::FLIP,
                    {cursor, 1000},
                    drawCtx,
                    {
                        .textSize = fontSize,
                        .buttonSize = buttonSize,
                        .borderRadius = glm::vec4(8.f),
                        .shadow = Core::Renderer::ShadowProps{.enabled = true},
                        .backgroundColor =
                            ViewportTheme::sceneWidgetsColors.surface.withAlpha(
                                isFlipped ? 1.f : 0.5f),
                    })) {
                style.schematicStyle.flipSlotsX = !isFlipped;
                const auto &simComp =
                    dynamic_cast<SimulationSceneComponent *>(selComp);
                BESS_ASSERT(simComp,
                            "Failed to cast selected component to "
                            "SimulationSceneComponent");
                simComp->setSchSlotsPosDirty(true);
            }
        }
    } // namespace

    void ScreenSpaceOverlayLayer::draw(SceneRenderContext &ctx) {
        return;
        if (!ctx.sceneState || !ctx.renderer || !ctx.camera) {
            return;
        }

        SceneDrawContext drawCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .camera = ctx.camera,
            .transformMode = Core::Renderer::RenderTransformMode::Screen,
            .viewportId = ctx.viewportCtx->viewportId,
            .sceneWidgetsState = ctx.sceneWidgetsState,
        };

        for (auto &callback : m_drawCallbacks) {
            if (callback) {
                callback(drawCtx, ctx);
            }
        }

        const auto &viewportSize = ctx.viewportCtx->transform.size;
        auto bottomRight = (viewportSize / 2.f) - padding;
        auto topLeft = (-viewportSize / 2.f) + padding;

        float xOffset = drawCameraZoom(drawCtx, ctx, bottomRight);
        bottomRight.x -= xOffset;
        drawCameraPos(drawCtx, ctx, bottomRight);

        const auto off = drawSchematicToggle(drawCtx, ctx, topLeft);
        drawSceneControls(drawCtx, ctx, off);

        auto topRight = glm::vec2{(viewportSize.x / 2.f) - padding,
                                  (-viewportSize.y / 2.f) + padding};
        drawSchematicViewControls(drawCtx, ctx, topRight);
    }

    void ScreenSpaceOverlayLayer::reset(SceneLifecycleContext &ctx) {
    }

    void ScreenSpaceOverlayLayer::destroy(SceneLifecycleContext &ctx) {
        clearDrawCallbacks();
    }

    void ScreenSpaceOverlayLayer::init(SceneLifecycleContext &ctx) {
        UI::View ui{ctx.sceneState};

        const auto containerStyle = UI::UIElementStyle{
            .backgroundColor =
                ViewportTheme::sceneWidgetsColors.surface.withAlpha(0.5f),
            .padding = padding,
            .margin = Core::Style::Margin::fromHorizontal(4.f),
            .shadow = Core::Renderer::ShadowProps{.enabled = true},
            .borderRadius = glm::vec4(8.f),
            .borderSize = Core::Style::BorderSize(0.f),
            .drawBg = true,
        };

        m_camPosXLabel =
            ui.label("",
                     UI::UIElementStyle{.padding = 0.f,
                                        .fontSize = fontSize,
                                        .widthMode = UI::LayoutSizeMode::point,
                                        .width = 90.f});

        m_camPosYLabel =
            ui.label("",
                     UI::UIElementStyle{.padding = 0.f,
                                        .margin = 0.f,
                                        .fontSize = fontSize,
                                        .widthMode = UI::LayoutSizeMode::point,
                                        .width = 90.f});

        m_camZoomLabel =
            ui.label("",
                     UI::UIElementStyle{.padding = 0.f,
                                        .fontSize = fontSize,
                                        .widthMode = UI::LayoutSizeMode::point,
                                        .width = 50.f});

        fmtCamPos({0.f, 0.f});
        fmtCamZoom(1.f);

        m_bottomContainer =
            ui.row(UI::
                       CompConfig{
                           .children =
                               {
                                   ui.button(
                                       UI::CompConfig{
                                           .children =
                                               {
                                                   ui.row(UI::CompConfig{
                                                       .children =
                                                           {
                                                               m_camPosXLabel,
                                                               m_camPosYLabel,
                                                           },
																													 .style = UI::UIElementStyle{
																														 .padding = 0.f,
																														 .margin = 0.f,
																														 .mainAxisAlignment =
																															 UI::LayoutAlignment::center,
																													 },
                                                   }),
                                               },
                                           .style = containerStyle,
                                       },
                                       []() {
                                           BESS_TRACE("[ScreenSpaceOverlayLayer"
                                                      "] Resetting "
                                                      "camera to origin");
                                       }),
                                   ui.row(UI::
                                              CompConfig{
                                                  .children =
                                                      {
                                                          m_camZoomLabel,
                                                      },
                                                  .style = containerStyle,
                                              }),
                               },

                           .style =
                               UI::UIElementStyle{
                                   .margin = 0,
                                   .mainAxisAlignment =
                                       UI::LayoutAlignment::end,
                                   .zVal = 5000.f,
                                   .drawPivot = UI::DrawPivot::bottomCenter,
                               },
                       });

        m_topContainer = ui.row(UI::CompConfig{
            .children =
                {
                    ui.row(UI::CompConfig{
                        .children =
                            {
                                ui.label("Top Overlay", {.fontSize = fontSize}),
                            },
                        .style = containerStyle,
                    }),
                },
            .style =
                UI::UIElementStyle{
                    .margin = 0,
                    .zVal = 5000.f,
                    .drawPivot = UI::DrawPivot::topCenter,
                },
        });

        m_topContainer->setIsScreenSpace();
        m_bottomContainer->setIsScreenSpace();
    }

    void ScreenSpaceOverlayLayer::addDrawCallback(DrawCallback callback) {
        m_drawCallbacks.push_back(std::move(callback));
    }

    void ScreenSpaceOverlayLayer::clearDrawCallbacks() {
        m_drawCallbacks.clear();
    }

    bool ScreenSpaceOverlayLayer::updateTransform(
        const std::shared_ptr<Core::Viewport::ViewportContext> &ctx) {

        BESS_ASSERT(m_topContainer, "Top container is null");
        BESS_ASSERT(m_bottomContainer, "Bottom container is null");

        if (ctx == nullptr || !m_topContainer || !m_bottomContainer ||
            !m_topContainer->getUINode() || !m_bottomContainer->getUINode()) {
            m_updateTransforms = true;
            return false;
        }

        const auto &size = ctx->transform.size;

        const auto topCenter = glm::vec2{
            0,
            -(size.y / 2.f),
        };

        const auto bottomCenter = glm::vec2{
            0,
            (size.y / 2.f),
        };

        m_topContainer->getUINode()->setPos(topCenter);
        m_bottomContainer->getUINode()->setPos(bottomCenter);
        m_topContainer->getUINode()->setWidth(size.x);
        m_bottomContainer->getUINode()->setWidth(size.x);

        m_updateTransforms = false;
        return true;
    }

    void ScreenSpaceOverlayLayer::fmtCamPos(const glm::vec2 &pos) {
        m_camPosXLabel->setName(
            std::format("{} {:>12.2f}", Icons::FontAwesomeIcons::FA_X, pos.x));
        m_camPosYLabel->setName(
            std::format("{} {:>12.2f}", Icons::FontAwesomeIcons::FA_Y, pos.y));
    }

    void ScreenSpaceOverlayLayer::fmtCamZoom(const float zoom) {
        m_camZoomLabel->setName(std::format("{:.2f}x", zoom));
    }

    void ScreenSpaceOverlayLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
    }

    void ScreenSpaceOverlayLayer::viewportUpdate(TimeMs ts,
                                                 SceneVpUpdateContext &ctx) {
        m_updateTransforms = ctx.viewportCtx->isResized || m_updateTransforms;
        if (m_updateTransforms) {
            updateTransform(ctx.viewportCtx);
        }

        // Very important to update for focused viewport only.
        // since same scene can be rendered in multiple viewports, we only want
        // to update the camera position label for the focused viewport
        if (m_camPosXLabel && ctx.viewportCtx->isFocused) {
            const auto &mPos = ctx.viewportCtx->inputCtx.mousePos;
            const glm::vec2 mouseWorldPos = ctx.camera->toWorldPos(mPos);
            fmtCamPos(mouseWorldPos);
        }

        if (m_camZoomLabel && ctx.viewportCtx->isFocused) {
            fmtCamZoom(ctx.camera->getZoom());
        }
    }

} // namespace Bess::Canvas
