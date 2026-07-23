#pragma once

#include "controls/basic_widgets.h"
#include "controls/scene_view.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui_composer.h"
#include "widget_ref.h"

#include <memory>
#include <string>

namespace Bess::Canvas {

    using namespace Bess::UI;

    class SceneViewport {
      public:
        SceneViewport() = default;

        void compose(Bess::UI::UIComposer &ui);

        SceneViewport(const SceneViewport &) = delete;
        SceneViewport &operator=(const SceneViewport &) = delete;

        [[nodiscard]] std::shared_ptr<Bess::UI::SceneViewportController>
        primaryViewport() const noexcept {
            return m_primaryViewport;
        }

      private:
        std::shared_ptr<Bess::UI::SceneViewportController> m_primaryViewport;
        Bess::UI::WidgetRef<Bess::UI::SceneView> m_sceneView;
        Bess::UI::WidgetRef<Label> m_camPosLabel;
        std::string m_lastCamPosText;
    };

} // namespace Bess::Canvas
