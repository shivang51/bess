#include "ui_painter.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    UIPainter::~UIPainter() = default;

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
            .zIndex = paint.zIndex,
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

        Core::Renderer::FontProps props{
            .fontSize = paint.fontSize,
            .color = paint.color,
            .zIndex = paint.zIndex,
            .letterSpacing = paint.letterSpacing,
            .id = paint.pickingId,
            .transformMode = Core::Renderer::RenderTransformMode::Screen,
        };
        const auto textSize = m_renderer.measureText(text, props);
        auto position = paint.bounds.topLeft();

        switch (paint.horizontal) {
        case HorizontalTextAlignment::start:
            break;
        case HorizontalTextAlignment::center:
            position.x += (paint.bounds.size.x - textSize.x) * 0.5f;
            break;
        case HorizontalTextAlignment::end:
            position.x += paint.bounds.size.x - textSize.x;
            break;
        }

        const float centerOffset = m_renderer.textCenterOffsetY(text, props);
        switch (paint.vertical) {
        case VerticalTextAlignment::start:
            position.y += centerOffset;
            break;
        case VerticalTextAlignment::center:
            position.y = paint.bounds.center.y + centerOffset;
            break;
        case VerticalTextAlignment::end:
            position.y += paint.bounds.size.y - textSize.y + centerOffset;
            break;
        }

        props.position = position;
        m_renderer.drawFont(text, props);
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
} // namespace Bess::UI
