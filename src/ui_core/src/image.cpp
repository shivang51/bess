#include "controls/image.h"

#include "layout.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>

namespace Bess::UI {
    namespace {
        constexpr float kImageContentZ = 0.001f;

        [[nodiscard]] float finiteNonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        [[nodiscard]] glm::vec2 finiteNonNegative(glm::vec2 value) noexcept {
            return {finiteNonNegative(value.x), finiteNonNegative(value.y)};
        }

        [[nodiscard]] float alignmentFactor(ImageAlignment alignment) noexcept {
            switch (alignment) {
            case ImageAlignment::start:
                return 0.f;
            case ImageAlignment::center:
                return 0.5f;
            case ImageAlignment::end:
                return 1.f;
            }
            return 0.5f;
        }

        [[nodiscard]] WidgetBounds alignedBounds(WidgetBounds slot,
                                                 glm::vec2 size,
                                                 ImageAlignment horizontal,
                                                 ImageAlignment vertical) {
            size = glm::max(size, glm::vec2{0.f});
            const glm::vec2 remaining = slot.size - size;
            const glm::vec2 topLeft =
                slot.topLeft() +
                glm::vec2{remaining.x * alignmentFactor(horizontal),
                          remaining.y * alignmentFactor(vertical)};
            return {.center = topLeft + size * 0.5f, .size = size};
        }

        [[nodiscard]] BoxPaint boxPaint(WidgetBounds bounds,
                                        const UIBoxStyle &style,
                                        PickingId pickingId) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .pickingId = pickingId,
            };
        }

        struct ResolvedImageGeometry {
            WidgetBounds bounds;
            glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
            bool clip = false;
        };

        [[nodiscard]] ResolvedImageGeometry
        resolveGeometry(WidgetBounds slot,
                        glm::vec2 textureSize,
                        const ImageOptions &options) {
            ResolvedImageGeometry result{.bounds = slot,
                                         .uvRect = options.uvRect};
            const glm::vec2 uvSpan{
                std::abs(options.uvRect.z - options.uvRect.x),
                std::abs(options.uvRect.w - options.uvRect.y),
            };
            const glm::vec2 sourceSize = textureSize * uvSpan;
            if (slot.empty() || sourceSize.x <= 0.f || sourceSize.y <= 0.f) {
                result.bounds.size = {0.f, 0.f};
                return result;
            }

            if (options.fit == ImageFit::fill) {
                return result;
            }

            const float containScale = std::min(slot.size.x / sourceSize.x,
                                                slot.size.y / sourceSize.y);
            if (options.fit == ImageFit::contain ||
                options.fit == ImageFit::scaleDown) {
                const float scale = options.fit == ImageFit::scaleDown
                                        ? std::min(1.f, containScale)
                                        : containScale;
                result.bounds = alignedBounds(slot,
                                              sourceSize * scale,
                                              options.horizontalAlignment,
                                              options.verticalAlignment);
                return result;
            }

            if (options.fit == ImageFit::none) {
                result.bounds = alignedBounds(slot,
                                              sourceSize,
                                              options.horizontalAlignment,
                                              options.verticalAlignment);
                result.clip = result.bounds.size.x > slot.size.x ||
                              result.bounds.size.y > slot.size.y;
                return result;
            }

            // Cover always draws inside the destination bounds and crops its
            // UV rectangle. This avoids geometry spilling into adjacent UI and
            // remains correct for reversed/mirrored UV axes.
            const float sourceAspect = sourceSize.x / sourceSize.y;
            const float targetAspect = slot.size.x / slot.size.y;
            glm::vec2 visibleFraction{1.f, 1.f};
            if (sourceAspect > targetAspect) {
                visibleFraction.x = targetAspect / sourceAspect;
            } else if (sourceAspect < targetAspect) {
                visibleFraction.y = sourceAspect / targetAspect;
            }

            const auto cropAxis =
                [](float first, float last, float visible, float alignment) {
                    const float span = last - first;
                    const float retained = span * visible;
                    const float offset = (span - retained) * alignment;
                    return std::pair{first + offset, first + offset + retained};
                };
            const auto [u0, u1] =
                cropAxis(options.uvRect.x,
                         options.uvRect.z,
                         visibleFraction.x,
                         alignmentFactor(options.horizontalAlignment));
            const auto [v0, v1] =
                cropAxis(options.uvRect.y,
                         options.uvRect.w,
                         visibleFraction.y,
                         alignmentFactor(options.verticalAlignment));
            result.uvRect = {u0, v0, u1, v1};
            return result;
        }
    } // namespace

    Image::Image(std::shared_ptr<Core::Renderer::ITexture> texture,
                 ImageOptions options)
        : m_texture(std::move(texture)),
          m_options(std::move(options)) {
    }

    Image::Image(ImageTextureProvider textureProvider, ImageOptions options)
        : m_textureProvider(std::move(textureProvider)),
          m_options(std::move(options)) {
    }

    std::string_view Image::typeName() const noexcept {
        return "Image";
    }

    WidgetTraits Image::traits() const noexcept {
        return {
            .focusable = false, .hitTestVisible = false, .clipChildren = false};
    }

    void Image::onMount(WidgetMountContext &context) {
        applyIntrinsicSize(context.layout, true);
    }

    void Image::updateLayout(WidgetLayoutContext &context) {
        if (m_intrinsicSizeDirty) {
            applyIntrinsicSize(context.layout, false);
        }
    }

    void Image::update(WidgetUpdateContext &context) {
        if (!m_options.autoSize) {
            return;
        }
        const auto observed = intrinsicSize();
        if (observed == m_lastObservedIntrinsicSize) {
            return;
        }
        m_lastObservedIntrinsicSize = observed;
        m_intrinsicSizeDirty = true;
        context.state.invalidate(
            context.id, WidgetInvalidation::layout | WidgetInvalidation::paint);
    }

    void Image::paint(WidgetPaintContext &context) const {
        auto texture = currentTexture();
        if (texture == nullptr || texture->getHandle() == 0) {
            if (m_options.placeholder.has_value()) {
                context.painter.drawBox(boxPaint(
                    context.bounds, *m_options.placeholder, context.pickingId));
            }
            return;
        }

        const auto geometry = resolveGeometry(
            context.bounds, finiteNonNegative(texture->getSize()), m_options);
        if (geometry.bounds.empty()) {
            return;
        }

        const ImagePaint paint{
            .bounds = geometry.bounds,
            .texture = std::move(texture),
            .uvRect = geometry.uvRect,
            .tint = m_options.tint,
            .cornerRadius = m_options.cornerRadius,
            .zIndex = kImageContentZ,
            .pickingId = context.pickingId,
        };
        if (geometry.clip) {
            const ScopedUIClip clip{context.painter, context.bounds};
            context.painter.drawImage(paint);
        } else {
            context.painter.drawImage(paint);
        }
    }

    const std::shared_ptr<Core::Renderer::ITexture> &
    Image::texture() const noexcept {
        return m_texture;
    }

    std::shared_ptr<Core::Renderer::ITexture> Image::currentTexture() const {
        return m_textureProvider ? m_textureProvider() : m_texture;
    }

    void Image::setTexture(std::shared_ptr<Core::Renderer::ITexture> texture) {
        if (!m_textureProvider && m_texture == texture) {
            // Texture implementations may finish loading or resize in place.
            // Re-setting the same shared object is an explicit refresh signal.
            m_intrinsicSizeDirty = true;
            return;
        }
        m_textureProvider = {};
        m_texture = std::move(texture);
        m_intrinsicSizeDirty = true;
    }

    void Image::setTextureProvider(ImageTextureProvider textureProvider) {
        m_texture.reset();
        m_textureProvider = std::move(textureProvider);
        m_intrinsicSizeDirty = true;
    }

    const ImageOptions &Image::options() const noexcept {
        return m_options;
    }

    void Image::setOptions(ImageOptions options) {
        m_options = std::move(options);
        m_intrinsicSizeDirty = true;
    }

    glm::vec2 Image::intrinsicSize() const {
        const auto texture = currentTexture();
        if (texture != nullptr && texture->getHandle() != 0) {
            const glm::vec2 uvSpan{
                std::abs(m_options.uvRect.z - m_options.uvRect.x),
                std::abs(m_options.uvRect.w - m_options.uvRect.y),
            };
            const glm::vec2 size = finiteNonNegative(texture->getSize());
            const glm::vec2 intrinsic = size * uvSpan;
            if (intrinsic.x > 0.f && intrinsic.y > 0.f) {
                return intrinsic;
            }
        }
        return finiteNonNegative(m_options.fallbackSize);
    }

    void Image::applyIntrinsicSize(LayoutNode &layout, bool initialMount) {
        if (!m_options.autoSize) {
            m_hasAppliedIntrinsicSize = false;
            m_ownsIntrinsicWidth = false;
            m_ownsIntrinsicHeight = false;
            m_intrinsicSizeDirty = false;
            return;
        }

        const auto size = intrinsicSize();
        if (initialMount) {
            m_ownsIntrinsicWidth = true;
            m_ownsIntrinsicHeight = true;
        } else if (m_hasAppliedIntrinsicSize) {
            constexpr float epsilon = 0.001f;
            m_ownsIntrinsicWidth =
                m_ownsIntrinsicWidth &&
                layout.getWidthMode() == LayoutSizeMode::point &&
                std::abs(layout.getWidthValue() -
                         m_lastAppliedIntrinsicSize.x) <= epsilon;
            m_ownsIntrinsicHeight =
                m_ownsIntrinsicHeight &&
                layout.getHeightMode() == LayoutSizeMode::point &&
                std::abs(layout.getHeightValue() -
                         m_lastAppliedIntrinsicSize.y) <= epsilon;
        } else {
            // Enabling intrinsic sizing after mount must not overwrite an
            // explicit caller dimension.
            m_ownsIntrinsicWidth =
                layout.getWidthMode() == LayoutSizeMode::auto_;
            m_ownsIntrinsicHeight =
                layout.getHeightMode() == LayoutSizeMode::auto_;
        }

        if (m_ownsIntrinsicWidth) {
            layout.setWidth(size.x);
        }
        if (m_ownsIntrinsicHeight) {
            layout.setHeight(size.y);
        }
        m_lastAppliedIntrinsicSize = size;
        m_lastObservedIntrinsicSize = size;
        m_hasAppliedIntrinsicSize = true;
        m_intrinsicSizeDirty = false;
    }

} // namespace Bess::UI
