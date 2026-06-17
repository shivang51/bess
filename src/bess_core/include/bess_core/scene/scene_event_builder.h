#pragma once

#include "camera.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_types.h"
#include <memory>
#include <vector>

namespace Bess {
    class InputSubSystem;
} // namespace Bess

namespace Bess::Canvas {
    class SceneEventBuilder {
      public:
        static std::vector<SceneEvent>
        buildFrameEvents(const InputSubSystem &inputSystem,
                         const std::shared_ptr<Camera> &camera,
                         const ViewportTransform &viewportTransform);
    };
} // namespace Bess::Canvas
