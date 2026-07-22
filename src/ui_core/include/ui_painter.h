#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "common/bess_api.h"
#include "ui_types.h"

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Bess::UI {

    enum class HorizontalTextAlignment : uint8_t { start, center, end };
    enum class VerticalTextAlignment : uint8_t { start, center, end };

    struct BoxPaint {
        WidgetBounds bounds;
        Core::Renderer::Color color{0.f, 0.f, 0.f, 0.f};
        Core::Renderer::Color borderColor{0.f, 0.f, 0.f, 0.f};
        glm::vec4 cornerRadius{0.f};
        glm::vec4 borderThickness{0.f};
        Core::Renderer::ShadowProps shadow{};
        float zIndex = 0.f;
        PickingId pickingId = PickingId::invalid();
    };

    struct TextPaint {
        WidgetBounds bounds;
        float fontSize = 14.f;
        Core::Renderer::Color color{1.f, 1.f, 1.f, 1.f};
        HorizontalTextAlignment horizontal = HorizontalTextAlignment::start;
        VerticalTextAlignment vertical = VerticalTextAlignment::center;
        float zIndex = 0.f;
        float letterSpacing = 0.f;
        PickingId pickingId = PickingId::invalid();
    };

    // A font-backed icon and its optional background are one visual unit.
    // Renderer-backed painters align both from the same prepared glyph origin
    // so bitmap pixel snapping cannot move the icon inside its affordance.
    struct IconPaint {
        TextPaint glyph;
        std::optional<BoxPaint> background;
    };

    // A sampled texture quad. Keeping a strong texture reference in the paint
    // command prevents a render-target resize or application-side handle swap
    // from invalidating the resource while a painter is consuming it.
    struct ImagePaint {
        WidgetBounds bounds;
        std::shared_ptr<Core::Renderer::ITexture> texture;
        // UV coordinates use a top-left origin and may be reversed to mirror
        // either axis. Values outside 0..1 retain the renderer's sampler
        // address-mode semantics.
        glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
        Core::Renderer::Color tint{1.f, 1.f, 1.f, 1.f};
        glm::vec4 cornerRadius{0.f};
        float zIndex = 0.f;
        PickingId pickingId = PickingId::invalid();
    };

    class BESS_API UIPainter {
      public:
        virtual ~UIPainter();

        [[nodiscard]] virtual glm::vec2 viewportSize() const noexcept = 0;
        virtual void drawBox(const BoxPaint &paint) = 0;
        virtual void drawText(std::string_view text,
                              const TextPaint &paint) = 0;
        virtual void drawIcon(std::string_view icon, const IconPaint &paint);
        // Defaulting to a no-op preserves lightweight measurement/test
        // painters. Rendering painters should override this operation.
        virtual void drawImage(const ImagePaint &paint);
        [[nodiscard]] virtual glm::vec2
        measureText(std::string_view text,
                    float fontSize,
                    float letterSpacing = 0.f) const = 0;
        virtual void pushClip(WidgetBounds bounds) = 0;
        virtual void popClip() = 0;
        // Adds a local Z offset for a widget subtree. Base implementations are
        // no-ops so lightweight test painters need not model depth.
        virtual void pushLayer(float zOffset);
        virtual void popLayer();
    };

    // Adapter from renderer-neutral widget painting to the shared renderer.
    // It borrows the renderer; renderer/device lifetime remains at UITarget's
    // renderer context boundary.
    class BESS_API RendererUIPainter final : public UIPainter {
      public:
        RendererUIPainter(
            Core::Renderer::IRenderer2D &renderer,
            glm::vec2 viewportSize,
            Core::Renderer::TextureHandle activeColorAttachment = 0,
            Core::Renderer::TextureHandle activePickingAttachment = 0) noexcept;

        [[nodiscard]] glm::vec2 viewportSize() const noexcept override;
        void drawBox(const BoxPaint &paint) override;
        void drawText(std::string_view text, const TextPaint &paint) override;
        void drawIcon(std::string_view icon, const IconPaint &paint) override;
        void drawImage(const ImagePaint &paint) override;
        [[nodiscard]] glm::vec2
        measureText(std::string_view text,
                    float fontSize,
                    float letterSpacing = 0.f) const override;
        void pushClip(WidgetBounds bounds) override;
        void popClip() override;
        void pushLayer(float zOffset) override;
        void popLayer() override;

      private:
        Core::Renderer::IRenderer2D &m_renderer;
        glm::vec2 m_viewportSize{0.f};
        // Draw commands carry backend handles, so retain their owning texture
        // objects until the painter (and therefore the renderer frame) ends.
        std::vector<std::shared_ptr<Core::Renderer::ITexture>>
            m_retainedTextures;
        Core::Renderer::TextureHandle m_activeColorAttachment = 0;
        Core::Renderer::TextureHandle m_activePickingAttachment = 0;
        std::vector<float> m_layerStack;
        float m_layerOffset = 0.f;
    };

    class ScopedUIClip {
      public:
        ScopedUIClip(UIPainter &painter, WidgetBounds bounds)
            : m_painter(&painter) {
            m_painter->pushClip(bounds);
        }

        ScopedUIClip(const ScopedUIClip &) = delete;
        ScopedUIClip &operator=(const ScopedUIClip &) = delete;

        ~ScopedUIClip() {
            m_painter->popClip();
        }

      private:
        UIPainter *m_painter;
    };

} // namespace Bess::UI
