#pragma once

// Inspired or mostly copied from
// https://github.com/material-foundation/material-color-utilities/blob/main/cpp/cam/hct.h

#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Style {
    using Vec3 = glm::highp_dvec3;
    typedef uint32_t Argb;

    // From:
    // https://github.com/material-foundation/material-color-utilities/blob/main/cpp/cam/cam.h
    struct Cam {
        double hue = 0.0;
        double chroma = 0.0;
        double j = 0.0;
        double q = 0.0;
        double m = 0.0;
        double s = 0.0;

        double jstar = 0.0;
        double astar = 0.0;
        double bstar = 0.0;
    };

    Cam CamFromInt(Argb argb);
    double LstarFromArgb(Argb argb);
    Argb SolveToInt(double hue_degrees, double chroma, double lstar);

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

        static constexpr HctColor fromColor(const Renderer::Color &color) {
            Argb argb = color.toARGB8();
            Cam cam = CamFromInt(argb);

            HctColor hct;
            hct.hue = cam.hue;
            hct.chroma = cam.chroma;
            hct.tone = LstarFromArgb(argb);
            return hct;
        }

        constexpr Renderer::Color toColor() const {
            return Renderer::Color::FromARGB(SolveToInt(hue, chroma, tone));
        }
    };
} // namespace Bess::Core::Style
