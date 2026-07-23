#pragma once

#include "controls/scene_view.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui_composer.h"
#include "widget_ref.h"
#include <memory>

namespace Bess::Canvas {

    using namespace Bess::UI;

    class SceneViewport {
      public:
        SceneViewport() = default;

        void compose(Bess::UI::UIComposer &ui);

        SceneViewport(const SceneViewport &) = delete;
        SceneViewport &operator=(const SceneViewport &) = delete;

      private:
        std::shared_ptr<Bess::UI::SceneViewportController> m_primaryViewport;
        Bess::UI::WidgetRef<Bess::UI::SceneView> m_sceneView;
    };

} // namespace Bess::Canvas
