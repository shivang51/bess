#include "scene_viewport.h"
#include "bess_core/g_app_context.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui_composer.h"
#include "widget_ref.h"

#include <cmath>
#include <format>
#include <memory>
#include <string>

namespace Bess::Canvas {

    namespace {
        [[nodiscard]] LayoutSpec stretchFill() {
            return {.width = LayoutLength::percent(100.f),
                    .height = LayoutLength::percent(100.f)};
        }

        [[nodiscard]] FlexContainerOptions shellColumn() {
            return {
                .direction = LayoutDirection::vertical,
                .mainAxisAlignment = LayoutAlignment::start,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
                .clipChildren = false,
                .hitTestVisible = false,
            };
        }

        [[nodiscard]] std::string formatCamPos(const glm::vec2 &pos) {
            return std::format("X: {:.1f}  Y: {:.1f}", pos.x, pos.y);
        }

        [[nodiscard]] glm::vec2 toViewportPos(
            const glm::vec2 &windowPos,
            const Bess::Core::Viewport::ViewportTransform &transform) {
            const glm::vec2 scale{
                std::isfinite(transform.inputScale.x) &&
                        transform.inputScale.x > 0.f
                    ? transform.inputScale.x
                    : 1.f,
                std::isfinite(transform.inputScale.y) &&
                        transform.inputScale.y > 0.f
                    ? transform.inputScale.y
                    : 1.f,
            };
            return {(windowPos.x - transform.pos.x) * scale.x,
                    (windowPos.y - transform.pos.y) * scale.y};
        }
    } // namespace

    void SceneViewport::compose(Bess::UI::UIComposer &ui) {
        m_primaryViewport = std::make_shared<Bess::UI::SceneViewportController>(
            "Scene Viewport");

        m_primaryViewport->setOnPostUpdate(
            [this](const Bess::UI::SceneViewportController::PostUpdateContext
                       &ctx) {
                const auto camera = ctx.controller.getCamera();
                const auto viewportCtx = ctx.controller.getViewportContext();
                if (!camera || !viewportCtx || !m_camPosLabel) {
                    return;
                }

                glm::vec2 viewportMouse = viewportCtx->inputCtx.mousePos;
                if (auto inputSystem =
                        Bess::GAppContext::getInstance()
                            .getSubSystem<Bess::InputSubSystem>()) {
                    viewportMouse = toViewportPos(
                        inputSystem->getMouseMoveState().pos,
                        viewportCtx->transform);
                }

                const auto text = formatCamPos(camera->toWorldPos(viewportMouse));
                if (text == m_lastCamPosText) {
                    return;
                }
                m_lastCamPosText = text;

                m_camPosLabel.mutate(
                    Bess::UI::WidgetInvalidation::layout |
                        Bess::UI::WidgetInvalidation::paint,
                    [text](Bess::UI::Label &label) { label.setText(text); });
            });

        auto stack = ui.stack([this](UIComposer &stack) {
            SceneViewOptions options;
            options.policy = RenderPolicy::whileVisible;
            options.focusable = true;
            options.hitTestVisible = true;
            options.cornerRadius = {};
            m_sceneView = stack.sceneView(m_primaryViewport, options);
            m_sceneView.setLayout(stretchFill());

            auto col = stack.column(shellColumn(), [this](UIComposer &col) {
                col.spacer();
                m_camPosLabel = col.label("", LabelOptions{.fontSize = 12.f});
            });
            col.setLayout(stretchFill());
        });
        stack.setLayout(stretchFill());
    }

} // namespace Bess::Canvas
