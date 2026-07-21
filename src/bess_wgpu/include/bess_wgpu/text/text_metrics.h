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
