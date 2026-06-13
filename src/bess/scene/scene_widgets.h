#pragma once

#include "common/types.h"
#include "scene_draw_context.h"
#include <glm.hpp>

namespace Bess::Canvas::SceneWidgets {
    void beginFrame();
    void endFrame();

    void registerWidget(const PickingId &id);
    bool contains(const PickingId &id);
    void queueClick(const PickingId &id);

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);
} // namespace Bess::Canvas::SceneWidgets
