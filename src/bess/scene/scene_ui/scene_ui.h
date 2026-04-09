#pragma once

#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
namespace Bess::Canvas::SceneUI {
    void drawToggleButton(const PickingId id,
                          const bool isHigh,
                          const glm::vec3 &buttonPos,
                          const glm::vec2 &buttonSize,
                          SceneDrawContext &context) {

        static const Renderer::QuadRenderProperties trackProps{
            .borderColor = ViewportTheme::colors.componentBorder,
            .borderRadius = glm::vec4(5.5f),
            .borderSize = glm::vec4(0.5f),
        };
        constexpr Renderer::QuadRenderProperties buttonProps{.borderRadius = glm::vec4(5.f)};
        // Button background / track
        context.materialRenderer->drawQuad(buttonPos,
                                           buttonSize,
                                           isHigh ? ViewportTheme::colors.stateHigh : ViewportTheme::colors.background,
                                           id,
                                           trackProps);

        // Toggle head
        const float buttonHeadPosX = isHigh
                                         ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                                         : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

        const glm::vec3 buttonHeadPos = glm::vec3(buttonHeadPosX,
                                                  buttonPos.y,
                                                  buttonPos.z);
        context.materialRenderer->drawQuad(buttonHeadPos,
                                           {buttonSize.y - 1.f, buttonSize.y - 1.f},
                                           ViewportTheme::colors.stateLow,
                                           id,
                                           buttonProps);
    }
} // namespace Bess::Canvas::SceneUI
