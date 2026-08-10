#include "bess_core/viewport.h"
#include "bess_core/scene/widgets/scene_widgets_internal.h"

namespace Bess::Core::Viewport {
    ViewportContext::ViewportContext()
        : sceneWidgetsState(
              std::make_unique<
                  Bess::Canvas::SceneWidgets::ViewportSceneWidgetsState>()) {
    }

    ViewportContext::~ViewportContext() = default;

    ViewportContext::ViewportContext(ViewportContext &&) noexcept = default;

    ViewportContext &
    ViewportContext::operator=(ViewportContext &&) noexcept = default;

    void ViewportContext::reset() {
        isFocused = false;

        mode = ViewportMode::normal;
        drawMode = ViewportDrawMode::none;
        connDrawCtx.reset();
        inputCtx.reset();
        pickingReadbackRequest.reset();
        selBoxCtx.reset();
        if (sceneWidgetsState) {
            sceneWidgetsState->clear();
        }
    }
} // namespace Bess::Core::Viewport
