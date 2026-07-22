#pragma once

#include "bess_core/renderer/texture.h"
#include "common/bess_api.h"
#include "ui_painter.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <memory>
#include <optional>

namespace Bess::UI {

    using ImageTextureProvider =
        std::function<std::shared_ptr<Core::Renderer::ITexture>()>;

    enum class ImageFit : uint8_t {
        // Ignore the source aspect ratio and occupy the complete widget.
        fill,
        // Preserve aspect ratio and fit completely inside the widget.
        contain,
        // Preserve aspect ratio and crop the source to fill the widget.
        cover,
        // Use the source's intrinsic pixel size, clipped by the widget.
        none,
        // Equivalent to contain, but never enlarge the source.
        scaleDown,
    };

    enum class ImageAlignment : uint8_t { start, center, end };

    struct ImageOptions {
        ImageFit fit = ImageFit::contain;
        ImageAlignment horizontalAlignment = ImageAlignment::center;
        ImageAlignment verticalAlignment = ImageAlignment::center;
        glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
        Core::Renderer::Color tint{1.f, 1.f, 1.f, 1.f};
        glm::vec4 cornerRadius{0.f};
        // When enabled, the image establishes its source pixel dimensions as
        // its initial layout size. Explicit LayoutNode values may still
        // override those dimensions after composition.
        bool autoSize = true;
        glm::vec2 fallbackSize{0.f};
        // Optional visual for an absent/uninitialized texture. No implicit
        // color is used so every visible color remains theme/application-owned.
        std::optional<UIBoxStyle> placeholder;
    };

    // Passive texture presentation. It owns no renderer/device state. A static
    // shared texture is appropriate for assets; use ImageTextureProvider for
    // render-target attachments that may be replaced during resize.
    class BESS_API Image : public Widget {
      public:
        explicit Image(std::shared_ptr<Core::Renderer::ITexture> texture = {},
                       ImageOptions options = {});
        explicit Image(ImageTextureProvider textureProvider,
                       ImageOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void update(WidgetUpdateContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] const std::shared_ptr<Core::Renderer::ITexture> &
        texture() const noexcept;
        // Resolves a dynamic provider, or returns the static texture. Use a
        // provider for attachments recreated by RenderSurface::resize().
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        currentTexture() const;
        void setTexture(std::shared_ptr<Core::Renderer::ITexture> texture);
        void setTextureProvider(ImageTextureProvider textureProvider);
        [[nodiscard]] const ImageOptions &options() const noexcept;
        void setOptions(ImageOptions options);

      private:
        [[nodiscard]] glm::vec2 intrinsicSize() const;
        void applyIntrinsicSize(LayoutNode &layout, bool initialMount);

        std::shared_ptr<Core::Renderer::ITexture> m_texture;
        ImageTextureProvider m_textureProvider;
        ImageOptions m_options;
        glm::vec2 m_lastAppliedIntrinsicSize{0.f};
        glm::vec2 m_lastObservedIntrinsicSize{0.f};
        bool m_hasAppliedIntrinsicSize = false;
        bool m_ownsIntrinsicWidth = false;
        bool m_ownsIntrinsicHeight = false;
        bool m_intrinsicSizeDirty = true;
    };

} // namespace Bess::UI
