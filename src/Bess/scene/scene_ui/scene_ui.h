#pragma once

#include "scene/scene_state/components/types.h"
namespace Bess::Canvas::SceneUI {
    void drawToggleButton(const PickingId id,
                          const bool isHigh,
                          const glm::vec3 &buttonPos,
                          const glm::vec2 &buttonSize,
                          const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer,
                          const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer) {

        constexpr Renderer::QuadRenderProperties buttonProps{.borderRadius = glm::vec4(4.f)};
        // Button background / track
        materialRenderer->drawQuad(buttonPos,
                                   buttonSize,
                                   isHigh ? ViewportTheme::colors.stateHigh : ViewportTheme::colors.background,
                                   id,
                                   buttonProps);

        // Toggle head
        const float buttonHeadPosX = isHigh
                                         ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                                         : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

        const glm::vec3 buttonHeadPos = glm::vec3(buttonHeadPosX,
                                                  buttonPos.y,
                                                  buttonPos.z);
        materialRenderer->drawQuad(buttonHeadPos,
                                   {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                   ViewportTheme::colors.stateLow,
                                   id,
                                   buttonProps);
    }
} // namespace Bess::Canvas::SceneUI
