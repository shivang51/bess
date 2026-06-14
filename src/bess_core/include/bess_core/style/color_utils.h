#pragma once

// Inspired or mostly copied from
// https://github.com/material-foundation/material-color-utilities/blob/main/cpp/cam/hct.h

#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Style {

    // Hue-Chroma-Tone color representation, used for color scheme generation
    // and manipulation.
    struct HctColor {
        double hue = 0.f;
        double chroma = 0.f;
        double tone = 0.f;

        constexpr HctColor() = default;

        constexpr HctColor(double h, double c, double t)
            : hue(h),
              chroma(c),
              tone(t) {
        }

        constexpr HctColor fromColor(const Renderer::Color &color);

        constexpr Renderer::Color toColor() const;
    };
} // namespace Bess::Core::Style
