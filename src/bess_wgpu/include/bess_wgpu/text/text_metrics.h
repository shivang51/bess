#pragma once

#include "bess_core/renderer/renderer_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace Bess::Wgpu::Text {

    // Metrics are expressed in the same coordinate space as FontProps. The
    // descender follows FreeType/MSDF convention and is normally negative.
    struct TextLineMetrics {
        float ascender = 0.f;
        float descender = 0.f;
        float lineHeight = 0.f;
    };

    // Converts an ink center expressed relative to the text origin into the
    // origin adjustment needed to center that visible ink inside the measured
    // text width. This matters for small hinted glyphs whose raster bounds can
    // extend beyond their typographic advance.
    [[nodiscard]] inline float centeredInkOriginOffsetX(
        float measuredWidth, float inkLeft, float inkRight) noexcept {
        if (!std::isfinite(measuredWidth) || measuredWidth <= 0.f ||
            !std::isfinite(inkLeft) || !std::isfinite(inkRight) ||
            inkRight < inkLeft) {
            return 0.f;
        }
        return measuredWidth * 0.5f - (inkLeft + inkRight) * 0.5f;
    }

    // Returns the baseline displacement that places raster ink bounds around
    // the requested center. `inkTop` and `inkBottom` are y-down coordinates
    // relative to the unshifted baseline.
    [[nodiscard]] inline float
    centeredInkBaselineOffsetY(float inkTop, float inkBottom) noexcept {
        if (!std::isfinite(inkTop) || !std::isfinite(inkBottom) ||
            inkBottom < inkTop) {
            return 0.f;
        }
        return -(inkTop + inkBottom) * 0.5f;
    }

    [[nodiscard]] constexpr std::size_t
    textLineCount(std::string_view text) noexcept {
        if (text.empty()) {
            return 0;
        }

        std::size_t lines = 1;
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '\r') {
                ++lines;
                if (i + 1 < text.size() && text[i + 1] == '\n') {
                    ++i;
                }
            } else if (text[i] == '\n') {
                ++lines;
            }
        }
        return lines;
    }

    // Returns the first baseline relative to the vertical center of a text
    // box. It deliberately uses font-wide metrics instead of the ink bounds
    // of the supplied glyphs: otherwise a descender in "Help" or "g" moves
    // that whole label relative to neighboring strings such as "File".
    [[nodiscard]] inline float
    centeredBaselineOffsetY(std::string_view text,
                            const Core::Renderer::FontProps &props,
                            TextLineMetrics metrics) noexcept {
        if (text.empty() || !std::isfinite(props.fontSize) ||
            props.fontSize <= 0.f) {
            return 0.f;
        }

        const float fontSize = std::max(props.fontSize, 1.f);
        if (!std::isfinite(metrics.ascender) ||
            !std::isfinite(metrics.descender) ||
            metrics.ascender <= metrics.descender) {
            // A conventional 80/20 em split is a deterministic fallback for
            // a renderer without usable font metrics.
            metrics.ascender = fontSize * 0.8f;
            metrics.descender = -fontSize * 0.2f;
        }

        float lineHeight = metrics.lineHeight;
        if (std::isfinite(props.lineHeight) && props.lineHeight > 0.f) {
            lineHeight = props.lineHeight;
        }
        if (!std::isfinite(lineHeight) || lineHeight <= 0.f) {
            lineHeight = fontSize;
        }
        lineHeight = std::max(lineHeight, fontSize);

        const auto lineCount = static_cast<float>(textLineCount(text));
        const float subsequentLines = std::max(0.f, lineCount - 1.f);
        return (metrics.ascender + metrics.descender -
                subsequentLines * lineHeight) *
               0.5f;
    }

} // namespace Bess::Wgpu::Text
