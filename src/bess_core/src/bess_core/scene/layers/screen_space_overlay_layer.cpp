#include "bess_core/scene/layers/screen_space_overlay_layer.h"
#include "bess_core/g_app_context.h"
#include "project_session/project_session.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/scene/scene_ui/ui_view.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/scene_driver.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "bess_core/scene/scene_component_types.h"
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
    } // namespace

    void ScreenSpaceOverlayLayer::draw(SceneRenderContext &ctx) {
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

        const auto onPosLabelClick = [this]() {
            if (m_camera) {
                m_camera->focusAtPoint({0.f, 0.f}, false);
            }
        };

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
                                       onPosLabelClick),
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
        auto toggleBtn = ui.toggleButton(
            "Schematic Mode",
            [this](bool toggled) { m_vpCtx->toggleSchematicMode(); },
            false,
            UI::UIElementStyle{
                .padding = 0.f,
                .margin = 0.f,
                .fontSize = fontSize,
            });

        m_topContainer = ui.row(UI::CompConfig{
            .children =
                {
                    ui.row(UI::CompConfig{
                        .children = {toggleBtn},
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

        const auto isHiddenCb = [](const SceneDrawContext &ctx) -> bool {
            return !ctx.viewportCtx->isFocused;
        };

        m_topContainer->setIsHiddenCb(isHiddenCb);
        m_bottomContainer->setIsHiddenCb(isHiddenCb);
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
        if (ctx.viewportCtx->isFocused) {
            m_camera = ctx.camera;
            m_vpCtx = ctx.viewportCtx;

            if (m_camPosXLabel) {
                const auto &mPos = ctx.viewportCtx->inputCtx.mousePos;
                const glm::vec2 mouseWorldPos = ctx.camera->toWorldPos(mPos);
                fmtCamPos(mouseWorldPos);
            }

            if (m_camZoomLabel && ctx.viewportCtx->isFocused) {
                fmtCamZoom(ctx.camera->getZoom());
            }
        }
    }

} // namespace Bess::Canvas
