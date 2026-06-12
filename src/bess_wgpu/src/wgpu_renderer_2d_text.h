#pragma once

#include "bess_core/renderer/font.h"
#include "bess_core/renderer/renderer_path.h"
#include "bess_core/renderer/renderer_types.h"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Bess::Wgpu::Renderer2DDetail {

    constexpr uint32_t kReplacementCodepoint = 0xFFFD;

    inline bool isUtf8ContinuationByte(char c) {
        const auto byte = static_cast<unsigned char>(c);
        return (byte & 0xC0) == 0x80;
    }

    inline uint32_t decodeUtf8(std::string_view text, size_t &offset) {
        if (offset >= text.size()) {
            return 0;
        }

        const size_t start = offset;
        const auto first = static_cast<unsigned char>(text[start]);
        if (first <= 0x7F) {
            offset = start + 1;
            return first;
        }

        uint32_t codepoint = 0;
        size_t length = 0;
        uint32_t minCodepoint = 0;
        if ((first & 0xE0) == 0xC0) {
            codepoint = first & 0x1F;
            length = 2;
            minCodepoint = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            codepoint = first & 0x0F;
            length = 3;
            minCodepoint = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            codepoint = first & 0x07;
            length = 4;
            minCodepoint = 0x10000;
        } else {
            offset = start + 1;
            return kReplacementCodepoint;
        }

        if (start + length > text.size()) {
            offset = start + 1;
            return kReplacementCodepoint;
        }

        for (size_t i = 1; i < length; ++i) {
            const char byte = text[start + i];
            if (!isUtf8ContinuationByte(byte)) {
                offset = start + 1;
                return kReplacementCodepoint;
            }
            codepoint =
                (codepoint << 6) | (static_cast<uint32_t>(byte) & 0x3F);
        }

        if (codepoint < minCodepoint || codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            offset = start + 1;
            return kReplacementCodepoint;
        }

        offset = start + length;
        return codepoint;
    }

    inline Core::Renderer::PathCommand transformTextCommand(
        const Core::Renderer::PathCommand &command, const glm::vec2 &origin,
        float scale) {
        using Core::Renderer::PathCommandKind;

        Core::Renderer::PathCommand transformed = command;
        const auto transformPoint = [&](const glm::vec2 &point) {
            return origin + (point * scale);
        };

        switch (command.kind) {
        case PathCommandKind::Move:
        case PathCommandKind::Line:
            transformed.p = transformPoint(command.p);
            break;
        case PathCommandKind::Quad:
            transformed.p = transformPoint(command.p);
            transformed.control = transformPoint(command.control);
            break;
        case PathCommandKind::Cubic:
            transformed.p = transformPoint(command.p);
            transformed.control = transformPoint(command.control);
            transformed.control2 = transformPoint(command.control2);
            break;
        case PathCommandKind::Close:
            break;
        }
        return transformed;
    }

    inline glm::vec2 measurePathText(
        std::string_view text, const Core::Renderer::FontProps &props,
        Core::Renderer::FontFile &font) {
        if (text.empty() || props.fontSize <= 0.f || font.getSize() <= 0.f) {
            return {0.f, 0.f};
        }

        const float scale = props.fontSize / font.getSize();
        const Core::Renderer::Glyph &spaceGlyph = font.getGlyph(U' ');
        const float spaceAdvance =
            std::max(spaceGlyph.advanceX * scale, props.fontSize * 0.25f);

        float lineAdvance = 0.f;
        float maxWidth = 0.f;
        float totalHeight = props.fontSize;

        auto finishLine = [&]() {
            maxWidth = std::max(maxWidth, lineAdvance);
            lineAdvance = 0.f;
        };

        size_t offset = 0;
        while (offset < text.size()) {
            const uint32_t codepoint = decodeUtf8(text, offset);
            if (codepoint == 0) {
                break;
            }

            if (codepoint == '\r') {
                if (offset < text.size() && text[offset] == '\n') {
                    ++offset;
                }
                finishLine();
                totalHeight += props.fontSize;
                continue;
            }

            if (codepoint == '\n') {
                finishLine();
                totalHeight += props.fontSize;
                continue;
            }

            if (codepoint == '\t') {
                lineAdvance +=
                    (spaceAdvance * std::max(props.tabSize, 1.f)) +
                    props.letterSpacing;
                continue;
            }

            const Core::Renderer::Glyph &glyph =
                font.getGlyph(static_cast<char32_t>(codepoint));
            const float advance =
                glyph.advanceX > 0.f
                    ? glyph.advanceX * scale
                    : std::max(glyph.width * scale, props.fontSize * 0.5f);
            lineAdvance += advance + props.letterSpacing;
        }

        finishLine();
        return {maxWidth, totalHeight};
    }

    inline float pathCenterOffsetY(std::string_view text,
                                   const Core::Renderer::FontProps &props,
                                   Core::Renderer::FontFile &font) {
        if (text.empty() || props.fontSize <= 0.f || font.getSize() <= 0.f) {
            return 0.f;
        }

        const float scale = props.fontSize / font.getSize();
        const float defaultLineHeight = font.lineHeight() * scale;
        const float lineHeight =
            props.lineHeight > 0.f
                ? props.lineHeight
                : (0.f < defaultLineHeight ? defaultLineHeight
                                           : props.fontSize);

        float baselineY = 0.f;
        float inkTop = std::numeric_limits<float>::max();
        float inkBottom = std::numeric_limits<float>::lowest();
        bool hasInk = false;

        size_t offset = 0;
        while (offset < text.size()) {
            const uint32_t codepoint = decodeUtf8(text, offset);
            if (codepoint == 0) {
                break;
            }

            if (codepoint == '\r') {
                if (offset < text.size() && text[offset] == '\n') {
                    ++offset;
                }
                baselineY += lineHeight;
                continue;
            }

            if (codepoint == '\n') {
                baselineY += lineHeight;
                continue;
            }

            if (codepoint == '\t') {
                continue;
            }

            const Core::Renderer::Glyph &glyph =
                font.getGlyph(static_cast<char32_t>(codepoint));
            const auto bounds = glyph.path.bounds();
            if (!bounds.valid) {
                continue;
            }

            inkTop = std::min(inkTop, baselineY + (bounds.min.y * scale));
            inkBottom =
                std::max(inkBottom, baselineY + (bounds.max.y * scale));
            hasInk = true;
        }

        return hasInk ? -((inkTop + inkBottom) * 0.5f)
                      : props.fontSize * 0.35f;
    }

} // namespace Bess::Wgpu::Renderer2DDetail
