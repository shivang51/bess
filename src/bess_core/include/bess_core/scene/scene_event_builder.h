#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_event.h"
#include "bess_core/viewport.h"
#include "camera.h"
#include <memory>
#include <vector>

namespace Bess {
    class InputSubSystem;
} // namespace Bess

namespace Bess::Canvas {
    class BESS_API SceneEventBuilder {
      public:
        static std::vector<SceneEvent> buildFrameEvents(
            const InputSubSystem &inputSystem,
            const std::shared_ptr<Camera> &camera,
            const Core::Viewport::ViewportTransform &viewportTransform);
    };
} // namespace Bess::Canvas
