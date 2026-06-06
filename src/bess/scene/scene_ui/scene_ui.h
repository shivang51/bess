#pragma once

#include "scene/scene_draw_helpers.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
namespace Bess::Canvas::SceneUI {
    void drawToggleButton(const PickingId id, const bool isHigh,
                          const glm::vec3 &buttonPos,
                          const glm::vec2 &buttonSize,
                          SceneDrawContext &context) {

        static const SceneDraw::QuadStyle trackProps{
            .borderColor = ViewportTheme::colors.componentBorder,
            .borderRadius = glm::vec4(5.5f),
            .borderSize = glm::vec4(0.5f),
        };
        constexpr SceneDraw::QuadStyle buttonProps{
            .borderRadius = glm::vec4(5.f)};
        // Button background / track
        SceneDraw::drawQuad(
            context,
            buttonPos, buttonSize,
            isHigh ? ViewportTheme::colors.stateHigh
                   : ViewportTheme::colors.background,
            id, trackProps);

        // Toggle head
        const float buttonHeadPosX =
            isHigh ? buttonPos.x + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                   : buttonPos.x - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

        const glm::vec3 buttonHeadPos =
            glm::vec3(buttonHeadPosX, buttonPos.y, buttonPos.z);
        SceneDraw::drawQuad(
            context,
            buttonHeadPos, {buttonSize.y - 1.f, buttonSize.y - 1.f},
            ViewportTheme::colors.stateLow, id, buttonProps);
    }
} // namespace Bess::Canvas::SceneUI
