#include "scene_viewport.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui_composer.h"
#include "widget_ref.h"
#include <memory>

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
    } // namespace

    void SceneViewport::compose(Bess::UI::UIComposer &ui) {
        m_primaryViewport = std::make_shared<Bess::UI::SceneViewportController>(
            "Scene Viewport");

        auto stack = ui.stack([this, &ui](UIComposer &stack) {
            SceneViewOptions options;
            options.policy = RenderPolicy::whileVisible;
            options.focusable = true;
            options.hitTestVisible = true;
            options.cornerRadius = {};
            m_sceneView = stack.sceneView(m_primaryViewport, options);
            // m_sceneView.setLayout(stretchFill());

            auto col = stack.column(shellColumn(), [](UIComposer &col) {
                col.spacer();
                col.label("Scene Viewport", LabelOptions{.fontSize = 12.f});
            });

            col.setLayout(stretchFill());
        });
    }

} // namespace Bess::Canvas
