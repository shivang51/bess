#include "ui_painter.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    namespace {
        [[nodiscard]] Core::Renderer::FontProps
        prepareText(Core::Renderer::IRenderer2D &renderer,
                    std::string_view text,
                    const TextPaint &paint,
                    float layerOffset) {
            Core::Renderer::FontProps props{
                .fontSize = paint.fontSize,
                .color = paint.color,
                .zIndex = paint.zIndex + layerOffset,
                .letterSpacing = paint.letterSpacing,
                .id = paint.pickingId,
                .transformMode = Core::Renderer::RenderTransformMode::Screen,
            };
            const auto textSize = renderer.measureText(text, props);
            auto position = paint.bounds.topLeft();

            switch (paint.horizontal) {
            case HorizontalTextAlignment::start:
                break;
            case HorizontalTextAlignment::center:
                position.x += (paint.bounds.size.x - textSize.x) * 0.5f;
                position.x += renderer.textCenterOffsetX(text, props);
                break;
            case HorizontalTextAlignment::end:
                position.x += paint.bounds.size.x - textSize.x;
                break;
            }

            const float centerOffset = renderer.textCenterOffsetY(text, props);
            switch (paint.vertical) {
            case VerticalTextAlignment::start:
                position.y += centerOffset;
                break;
            case VerticalTextAlignment::center:
                position.y = paint.bounds.center.y + centerOffset;
                break;
            case VerticalTextAlignment::end:
                position.y +=
                    paint.bounds.size.y - textSize.y + centerOffset;
                break;
            }

            props.position = position;
            return props;
        }

        [[nodiscard]] glm::vec2
        pixelSnapDelta(glm::vec2 screenSpaceOrigin,
                       glm::vec2 viewportSize) noexcept {
            if (!std::isfinite(screenSpaceOrigin.x) ||
                !std::isfinite(screenSpaceOrigin.y)) {
                return {0.f, 0.f};
            }
            const glm::vec2 viewport =
                glm::max(viewportSize, glm::vec2{0.f});
            const glm::vec2 pixel = screenSpaceOrigin + viewport * 0.5f;
            return {std::round(pixel.x) - pixel.x,
                    std::round(pixel.y) - pixel.y};
        }
    } // namespace

    UIPainter::~UIPainter() = default;

    void UIPainter::drawIcon(std::string_view icon, const IconPaint &paint) {
        if (paint.background) {
            drawBox(*paint.background);
        }
        drawText(icon, paint.glyph);
    }

    void UIPainter::pushLayer(float) {
    }

    void UIPainter::popLayer() {
    }

    RendererUIPainter::RendererUIPainter(Core::Renderer::IRenderer2D &renderer,
                                         glm::vec2 viewportSize) noexcept
        : m_renderer(renderer),
          m_viewportSize(glm::max(viewportSize, glm::vec2{0.f})) {
    }

    glm::vec2 RendererUIPainter::viewportSize() const noexcept {
        return m_viewportSize;
    }

    void RendererUIPainter::drawBox(const BoxPaint &paint) {
        if (paint.bounds.empty() ||
            (paint.color.a <= 0.f && paint.borderColor.a <= 0.f)) {
            return;
        }

        m_renderer.drawQuad({
            .position = paint.bounds.center,
            .size = paint.bounds.size,
            .zIndex = paint.zIndex + m_layerOffset,
            .color = paint.color,
            .id = paint.pickingId,
            .transformMode = Core::Renderer::RenderTransformMode::Screen,
            .radius = paint.cornerRadius,
            .thickness = paint.borderThickness,
            .borderColor = paint.borderColor,
            .shadow = paint.shadow,
        });
    }

    void RendererUIPainter::drawText(std::string_view text,
                                     const TextPaint &paint) {
        if (text.empty() || paint.bounds.empty() || paint.fontSize <= 0.f ||
            paint.color.a <= 0.f) {
            return;
        }

        const auto props =
            prepareText(m_renderer, text, paint, m_layerOffset);
        m_renderer.drawFont(text, props);
    }

    void RendererUIPainter::drawIcon(std::string_view icon,
                                     const IconPaint &paint) {
        const auto &glyph = paint.glyph;
        if (icon.empty() || glyph.bounds.empty() || glyph.fontSize <= 0.f ||
            glyph.color.a <= 0.f) {
            if (paint.background) {
                drawBox(*paint.background);
            }
            return;
        }

        auto props = prepareText(m_renderer, icon, glyph, m_layerOffset);
        const glm::vec2 delta =
            pixelSnapDelta(props.position, m_viewportSize);
        props.position += delta;

        if (paint.background) {
            auto background = *paint.background;
            background.bounds.center += delta;
            drawBox(background);
        }
        m_renderer.drawFont(icon, props);
    }

    glm::vec2 RendererUIPainter::measureText(std::string_view text,
                                             float fontSize,
                                             float letterSpacing) const {
        return m_renderer.measureText(text,
                                      {
                                          .fontSize = fontSize,
                                          .letterSpacing = letterSpacing,
                                      });
    }

    void RendererUIPainter::pushClip(WidgetBounds bounds) {
        const auto viewport = glm::max(m_viewportSize, glm::vec2{0.f});
        const auto topLeft = bounds.topLeft() + viewport * 0.5f;
        const auto bottomRight = bounds.bottomRight() + viewport * 0.5f;

        const float x0 = std::clamp(topLeft.x, 0.f, viewport.x);
        const float y0 = std::clamp(topLeft.y, 0.f, viewport.y);
        const float x1 = std::clamp(bottomRight.x, 0.f, viewport.x);
        const float y1 = std::clamp(bottomRight.y, 0.f, viewport.y);

        const auto left = static_cast<uint32_t>(std::floor(x0));
        const auto top = static_cast<uint32_t>(std::floor(y0));
        const auto right = static_cast<uint32_t>(std::ceil(x1));
        const auto bottom = static_cast<uint32_t>(std::ceil(y1));
        m_renderer.pushScissorRect({
            .x = left,
            .y = top,
            .width = right > left ? right - left : 0u,
            .height = bottom > top ? bottom - top : 0u,
        });
    }

    void RendererUIPainter::popClip() {
        m_renderer.popScissorRect();
    }

    void RendererUIPainter::pushLayer(float zOffset) {
        m_layerStack.push_back(m_layerOffset);
        if (std::isfinite(zOffset)) {
            m_layerOffset += zOffset;
        }
    }

    void RendererUIPainter::popLayer() {
        if (m_layerStack.empty()) {
            m_layerOffset = 0.f;
            return;
        }
        m_layerOffset = m_layerStack.back();
        m_layerStack.pop_back();
    }
} // namespace Bess::UI
