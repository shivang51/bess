#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/types.h"
#include "scene_draw_context.h"
#include <glm.hpp>

namespace Bess::Canvas::SceneWidgets {
    void beginFrame();
    void endFrame();

    bool contains(const PickingId &id);
    void queueClick(const PickingId &id);
    void setHoverId(const PickingId &id);

    bool toggleButton(const PickingId &id, bool value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool toggleButton(const PickingId &id, bool *value,
                      const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                      SceneDrawContext &context);

    bool button(const PickingId &id, const std::string &label,
                const glm::vec3 &buttonPos, const glm::vec2 &buttonSize,
                const Core::Renderer::Color &labelColor,
                SceneDrawContext &context);
} // namespace Bess::Canvas::SceneWidgets
