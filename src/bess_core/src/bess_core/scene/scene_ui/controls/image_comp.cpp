#include "bess_core/scene/scene_ui/controls/image_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kImageInfo = 1u;
        constexpr float kMinImageSide = 1.f;

        [[nodiscard]] UIImageTextureLoader &defaultTextureLoaderStorage() {
            static UIImageTextureLoader loader;
            return loader;
        }

        [[nodiscard]] bool validSize(const glm::vec2 &size) {
            return std::isfinite(size.x) && std::isfinite(size.y) &&
                   size.x > 0.f && size.y > 0.f;
        }

        [[nodiscard]] glm::vec2 sanitizeSize(const glm::vec2 &size,
                                             const glm::vec2 &fallback) {
            glm::vec2 result = size;
            if (!std::isfinite(result.x) || result.x <= 0.f) {
                result.x = fallback.x;
            }
            if (!std::isfinite(result.y) || result.y <= 0.f) {
                result.y = fallback.y;
            }
            result.x = std::max(kMinImageSide, result.x);
            result.y = std::max(kMinImageSide, result.y);
            return result;
        }

        [[nodiscard]] glm::vec4 subTextureToUvRect(
            const Core::Renderer::SubTexture &subTexture) {
            const auto &startWH = subTexture.getStartWH();
            return {startWH.x,
                    startWH.y,
                    startWH.x + startWH.z,
                    startWH.y + startWH.w};
        }

        [[nodiscard]] glm::vec4 sanitizeUvRect(glm::vec4 uv) {
            uv.x = std::clamp(uv.x, 0.f, 1.f);
            uv.y = std::clamp(uv.y, 0.f, 1.f);
            uv.z = std::clamp(uv.z, 0.f, 1.f);
            uv.w = std::clamp(uv.w, 0.f, 1.f);

            if (uv.z < uv.x) {
                std::swap(uv.x, uv.z);
            }
            if (uv.w < uv.y) {
                std::swap(uv.y, uv.w);
            }
            if (uv.z == uv.x) {
                uv.x = 0.f;
                uv.z = 1.f;
            }
            if (uv.w == uv.y) {
                uv.y = 0.f;
                uv.w = 1.f;
            }
            return uv;
        }

        [[nodiscard]] bool validRgbaPayload(std::span<const uint8_t> rgba8,
                                            uint32_t width,
                                            uint32_t height) {
            if (rgba8.empty() || width == 0u || height == 0u) {
                return false;
            }

            const auto expected =
                static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            return rgba8.size() >= expected;
        }
    } // namespace

    std::shared_ptr<ImageComp> ImageComp::create(const glm::vec2 &imageSize) {
        auto image = std::make_shared<ImageComp>();
        image->setImageSize(imageSize);
        return image;
    }

    std::shared_ptr<ImageComp> ImageComp::create(
        const std::shared_ptr<Core::Renderer::ITexture> &texture,
        const glm::vec2 &imageSize) {
        auto image = std::make_shared<ImageComp>();
        image->setImageSize(imageSize);
        image->setTexture(texture);
        return image;
    }

    std::shared_ptr<ImageComp>
    ImageComp::create(Core::Renderer::TextureHandle texture,
                      const glm::vec2 &sourceSize,
                      const glm::vec2 &imageSize) {
        auto image = std::make_shared<ImageComp>();
        image->setImageSize(imageSize);
        image->setTextureHandle(texture, sourceSize);
        return image;
    }

    void ImageComp::setDefaultTextureLoader(UIImageTextureLoader loader) {
        defaultTextureLoaderStorage() = std::move(loader);
    }

    const UIImageTextureLoader &ImageComp::getDefaultTextureLoader() {
        return defaultTextureLoaderStorage();
    }

    void ImageComp::clearImage() {
        m_texture = nullptr;
        m_textureHandle = 0;
        m_sourceSize = {0.f, 0.f};
        m_uvRect = {0.f, 0.f, 1.f, 1.f};
        m_sourceRequest = {};
        m_pendingResolve = false;
        makeUIDirty();
    }

    void ImageComp::setTexture(
        const std::shared_ptr<Core::Renderer::ITexture> &texture) {
        const glm::vec2 sourceSize =
            texture != nullptr ? texture->getSize() : glm::vec2{0.f};
        assignTexture(texture, {0.f, 0.f, 1.f, 1.f}, sourceSize);
        m_sourceRequest = {};
        m_pendingResolve = false;
    }

    void
    ImageComp::setTextureHandle(Core::Renderer::TextureHandle texture,
                                const glm::vec2 &sourceSize) {
        m_texture = nullptr;
        m_textureHandle = texture;
        m_sourceSize = sourceSize;
        m_uvRect = {0.f, 0.f, 1.f, 1.f};
        m_sourceRequest = {};
        m_pendingResolve = false;
        makeUIDirty();
    }

    void ImageComp::setSubTexture(
        const std::shared_ptr<Core::Renderer::ITexture> &texture,
        const Core::Renderer::SubTexture &subTexture) {
        assignTexture(texture,
                      subTextureToUvRect(subTexture),
                      subTexture.getPixelSize());
        m_sourceRequest = {};
        m_pendingResolve = false;
    }

    void ImageComp::setSubTexture(Core::Renderer::TextureHandle texture,
                                  const Core::Renderer::SubTexture &subTexture) {
        m_texture = nullptr;
        m_textureHandle = texture;
        m_sourceSize = subTexture.getPixelSize();
        m_uvRect = sanitizeUvRect(subTextureToUvRect(subTexture));
        m_sourceRequest = {};
        m_pendingResolve = false;
        makeUIDirty();
    }

    void ImageComp::setSourceFile(const std::string &path) {
        m_sourceRequest = {};
        m_sourceRequest.type = UIImageSourceType::File;
        m_sourceRequest.path = path;
        markSourcePending();
        resolveTexture();
    }

    bool ImageComp::setPixelData(std::span<const uint8_t> rgba8,
                                 uint32_t width,
                                 uint32_t height) {
        if (!validRgbaPayload(rgba8, width, height)) {
            clearImage();
            return false;
        }

        const auto expected =
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
        std::vector<uint8_t> pixels;
        pixels.assign(rgba8.begin(), rgba8.begin() + expected);
        return setPixelData(std::move(pixels), width, height);
    }

    bool ImageComp::setPixelData(std::vector<uint8_t> rgba8,
                                 uint32_t width,
                                 uint32_t height) {
        if (!validRgbaPayload(rgba8, width, height)) {
            clearImage();
            return false;
        }

        const auto expected =
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
        if (rgba8.size() > expected) {
            rgba8.resize(expected);
        }

        m_sourceRequest = {};
        m_sourceRequest.type = UIImageSourceType::Pixels;
        m_sourceRequest.pixels.rgba8 = std::move(rgba8);
        m_sourceRequest.pixels.width = width;
        m_sourceRequest.pixels.height = height;
        m_sourceSize = {static_cast<float>(width), static_cast<float>(height)};
        markSourcePending();
        return resolveTexture();
    }

    bool ImageComp::resolveTexture() {
        if (!m_pendingResolve ||
            m_sourceRequest.type == UIImageSourceType::None) {
            return hasDrawableTexture();
        }

        const auto &loader =
            m_textureLoader ? m_textureLoader : getDefaultTextureLoader();
        if (!loader) {
            return false;
        }

        try {
            auto texture = loader(m_sourceRequest);
            if (texture == nullptr || texture->getHandle() == 0) {
                return false;
            }

            assignTexture(texture,
                          {0.f, 0.f, 1.f, 1.f},
                          texture->getSize());
            m_pendingResolve = false;
            return true;
        } catch (const std::exception &) {
            m_texture = nullptr;
            m_textureHandle = 0;
            m_pendingResolve = false;
            return false;
        }
    }

    void ImageComp::setTextureLoader(UIImageTextureLoader loader) {
        m_textureLoader = std::move(loader);
        resolveTexture();
    }

    void ImageComp::onDraw(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        const auto id = imagePickingId();
        const bool hasTexture = hasDrawableTexture();
        if (m_drawBackground || m_drawBorder ||
            (!hasTexture && m_drawPlaceholder)) {
            drawBackground(state, id, !hasTexture && m_drawPlaceholder);
        }

        if (!hasTexture) {
            if (m_drawPlaceholder) {
                drawPlaceholder(state, id);
            }
            return;
        }

        const auto frame = resolveDrawFrame();
        Core::Renderer::QuadProps props;
        props.position = frame.position;
        props.size = frame.size;
        props.zIndex = m_node->getDrawPos().z + 0.0001f;
        props.color = m_tintColor;
        props.texture = m_textureHandle;
        props.uvRect = frame.uvRect;
        props.radius = m_cornerRadius;
        props.id = id;
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void ImageComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());
        resolveTexture();

        const auto size = resolveLayoutSize();
        m_node->setWidth(size.x);
        m_node->setHeight(size.y);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool ImageComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (!m_interactive) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press &&
            (e.details == 0u || e.details == kImageInfo)) {
            if (m_clickCallback) {
                m_clickCallback(e);
            }
            return true;
        }

        return e.action == Events::MouseClickAction::release;
    }

    Core::Viewport::SceneCursor ImageComp::getCursor() const {
        return m_interactive ? Core::Viewport::SceneCursor::pointer
                             : Core::Viewport::SceneCursor::normal;
    }

    glm::vec2 ImageComp::resolveLayoutSize() const {
        const auto sourceSize = resolveSourceSize();
        glm::vec2 size = m_imageSize;

        if (size.x <= 0.f && size.y > 0.f && validSize(sourceSize)) {
            size.x = size.y * (sourceSize.x / sourceSize.y);
        }
        if (size.y <= 0.f && size.x > 0.f && validSize(sourceSize)) {
            size.y = size.x * (sourceSize.y / sourceSize.x);
        }

        const glm::vec2 fallback =
            validSize(sourceSize) ? sourceSize : glm::vec2{64.f, 64.f};
        return sanitizeSize(size, fallback);
    }

    glm::vec2 ImageComp::resolveSourceSize() const {
        if (validSize(m_sourceSize)) {
            return m_sourceSize;
        }
        if (m_texture != nullptr && validSize(m_texture->getSize())) {
            return m_texture->getSize();
        }
        return {0.f, 0.f};
    }

    ImageComp::DrawImageFrame ImageComp::resolveDrawFrame() const {
        DrawImageFrame frame;
        frame.position = m_node->getDrawPos();
        frame.size = m_node->getDrawSize();
        frame.uvRect = sanitizeUvRect(m_uvRect);

        const auto sourceSize = resolveSourceSize();
        if (!validSize(sourceSize) || !validSize(frame.size)) {
            return frame;
        }

        const float sourceAspect = sourceSize.x / sourceSize.y;
        const float boundsAspect = frame.size.x / frame.size.y;

        if (m_fit == UIImageFit::Stretch) {
            return frame;
        }

        if (m_fit == UIImageFit::None) {
            frame.size.x = std::min(frame.size.x, sourceSize.x);
            frame.size.y = std::min(frame.size.y, sourceSize.y);
            return frame;
        }

        if (m_fit == UIImageFit::Contain) {
            if (sourceAspect > boundsAspect) {
                frame.size.y = frame.size.x / sourceAspect;
            } else {
                frame.size.x = frame.size.y * sourceAspect;
            }
            return frame;
        }

        const float u0 = frame.uvRect.x;
        const float v0 = frame.uvRect.y;
        const float u1 = frame.uvRect.z;
        const float v1 = frame.uvRect.w;
        const float uvWidth = u1 - u0;
        const float uvHeight = v1 - v0;

        if (sourceAspect > boundsAspect) {
            const float visibleWidth = uvWidth * (boundsAspect / sourceAspect);
            const float inset = (uvWidth - visibleWidth) * 0.5f;
            frame.uvRect.x = u0 + inset;
            frame.uvRect.z = u1 - inset;
        } else {
            const float visibleHeight =
                uvHeight * (sourceAspect / boundsAspect);
            const float inset = (uvHeight - visibleHeight) * 0.5f;
            frame.uvRect.y = v0 + inset;
            frame.uvRect.w = v1 - inset;
        }

        return frame;
    }

    PickingId ImageComp::imagePickingId() const {
        if (!m_interactive) {
            return PickingId::invalid();
        }

        return PickingId{
            .runtimeId = resolveRuntimeId(),
            .info = kImageInfo,
        };
    }

    bool ImageComp::hasDrawableTexture() const {
        return m_textureHandle != 0;
    }

    void ImageComp::assignTexture(
        const std::shared_ptr<Core::Renderer::ITexture> &texture,
        const glm::vec4 &uvRect,
        const glm::vec2 &sourceSize) {
        m_texture = texture;
        m_textureHandle = texture != nullptr ? texture->getHandle() : 0;
        m_sourceSize = validSize(sourceSize) ? sourceSize : glm::vec2{0.f};
        m_uvRect = sanitizeUvRect(uvRect);
        makeUIDirty();
    }

    void ImageComp::drawBackground(SceneDrawContext &state,
                                   const PickingId &id,
                                   bool forcePlaceholderBackground) const {
        Core::Renderer::QuadProps props;
        props.position = m_node->getDrawPos();
        props.size = m_node->getDrawSize();
        props.zIndex = m_node->getDrawPos().z;
        props.color = forcePlaceholderBackground
                          ? m_style.backgroundColor.withAlpha(0.36f)
                          : m_style.backgroundColor;
        props.borderColor =
            m_drawBorder ? m_style.borderColor
                         : Core::Renderer::Color{0.f, 0.f, 0.f, 0.f};
        props.thickness =
            m_drawBorder ? m_style.metrics.borderSize.toVec4()
                         : glm::vec4{0.f};
        props.radius = m_cornerRadius;
        props.id = id;
        props.transformMode = state.transformMode;
        state.renderer->drawQuad(props);
    }

    void ImageComp::drawPlaceholder(SceneDrawContext &state,
                                    const PickingId &id) const {
        const auto pos = m_node->getDrawPos();
        const auto size = m_node->getDrawSize();
        const auto half = size * 0.5f;
        const float inset = std::max(3.f, std::min(size.x, size.y) * 0.16f);
        const auto color = m_style.borderColor.withAlpha(0.72f);

        Core::Renderer::LineProps line;
        line.thickness = 1.f;
        line.zIndex = pos.z + 0.0001f;
        line.color = color;
        line.id = id;
        line.transformMode = state.transformMode;

        line.p0 = {pos.x - half.x + inset, pos.y - half.y + inset};
        line.p1 = {pos.x + half.x - inset, pos.y + half.y - inset};
        state.renderer->drawLine(line);

        line.p0 = {pos.x - half.x + inset, pos.y + half.y - inset};
        line.p1 = {pos.x + half.x - inset, pos.y - half.y + inset};
        state.renderer->drawLine(line);
    }

    void ImageComp::markSourcePending() {
        m_texture = nullptr;
        m_textureHandle = 0;
        m_uvRect = {0.f, 0.f, 1.f, 1.f};
        m_pendingResolve = true;
        makeUIDirty();
    }
} // namespace Bess::Canvas::UI
