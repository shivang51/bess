#pragma once

#include "bess_core/renderer/renderer_types.h"
#include "common/bess_api.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"

namespace Bess::UI {

    struct UIBoxStyle {
        Core::Renderer::Color background{0.f, 0.f, 0.f, 0.f};
        Core::Renderer::Color border{0.f, 0.f, 0.f, 0.f};
        glm::vec4 cornerRadius{0.f};
        glm::vec4 borderThickness{0.f};
        Core::Renderer::ShadowProps shadow{};
    };

    struct UITextStyle {
        Core::Renderer::Color color{1.f, 1.f, 1.f, 1.f};
        float fontSize = 14.f;
        float letterSpacing = 0.f;
    };

    struct UIInteractiveStyle {
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle pressed;
        UIBoxStyle focused;
        UIBoxStyle disabled;
        UITextStyle text;
        Core::Renderer::Color disabledText{0.55f, 0.57f, 0.62f, 1.f};
        glm::vec2 minimumSize{72.f, 32.f};
        glm::vec2 contentPadding{12.f, 7.f};
    };

    struct UITabStyle {
        UIBoxStyle strip;
        UIBoxStyle normal;
        UIBoxStyle hovered;
        UIBoxStyle active;
        UIBoxStyle pressed;
        UITextStyle text;
        Core::Renderer::Color inactiveText{0.72f, 0.74f, 0.78f, 1.f};
        float height = 34.f;
        float minimumWidth = 72.f;
        float maximumWidth = 220.f;
        float horizontalPadding = 12.f;
    };

    struct UIDockStyle {
        UIBoxStyle background;
        UIBoxStyle stack;
        Core::Renderer::Color splitter{0.18f, 0.19f, 0.23f, 1.f};
        Core::Renderer::Color splitterHovered{0.32f, 0.55f, 0.95f, 1.f};
        float splitterThickness = 4.f;
    };

    struct BESS_API UITheme {
        UIBoxStyle panel;
        UITextStyle label;
        UIInteractiveStyle button;
        UITabStyle tabs;
        UIDockStyle dock;

        [[nodiscard]] static UITheme dark();
    };

} // namespace Bess::UI
