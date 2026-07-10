#pragma once

#include "bess_core/renderer/subtexture.h"
#include "bess_core/renderer/texture.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace Bess::Canvas::UI {

    enum class UIImageFit {
        Stretch,
        Contain,
        Cover,
        None,
    };

    enum class UIImageSourceType {
        None,
        File,
        Pixels,
    };

    struct UIImagePixelData {
        std::vector<uint8_t> rgba8;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct UIImageSourceRequest {
        UIImageSourceType type = UIImageSourceType::None;
        std::string path;
        UIImagePixelData pixels;
    };

    using UIImageTextureLoader = std::function<std::shared_ptr<
        Core::Renderer::ITexture>(const UIImageSourceRequest &)>;
    using UIImageClickCallback =
        std::function<void(const Events::MouseButtonEvent &)>;

    class ImageComp : public UISceneComponent {
      public:
        DEFAULT_CONTRS(ImageComp)

        static std::shared_ptr<ImageComp>
        create(const glm::vec2 &imageSize = {64.f, 64.f});
        static std::shared_ptr<ImageComp>
        create(const std::shared_ptr<Core::Renderer::ITexture> &texture,
               const glm::vec2 &imageSize = {0.f, 0.f});
        static std::shared_ptr<ImageComp>
        create(Core::Renderer::TextureHandle texture,
               const glm::vec2 &sourceSize,
               const glm::vec2 &imageSize = {0.f, 0.f});

        template <typename TTexture>
        static std::shared_ptr<ImageComp>
        createFromFile(const std::string &path,
                       const glm::vec2 &imageSize = {0.f, 0.f}) {
            static_assert(std::is_base_of_v<Core::Renderer::ITexture,
                                            TTexture>,
                          "TTexture must derive from ITexture");
            auto texture = std::make_shared<TTexture>(path);
            texture->init();
            auto image = create(texture, imageSize);
            image->m_sourceRequest.type = UIImageSourceType::File;
            image->m_sourceRequest.path = path;
            image->m_pendingResolve = false;
            return image;
        }

        template <typename TTexture>
        static std::shared_ptr<ImageComp>
        createFromPixels(std::span<const uint8_t> rgba8,
                         uint32_t width,
                         uint32_t height,
                         const glm::vec2 &imageSize = {0.f, 0.f}) {
            static_assert(std::is_base_of_v<Core::Renderer::ITexture,
                                            TTexture>,
                          "TTexture must derive from ITexture");
            auto image = create(imageSize);
            const auto expected =
                static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
            if (rgba8.empty() || width == 0u || height == 0u ||
                rgba8.size() < expected) {
                return image;
            }
            auto texture = TTexture::fromPixels(rgba8.data(), width, height);
            image->setTexture(texture);
            image->m_sourceRequest.type = UIImageSourceType::Pixels;
            image->m_sourceRequest.pixels.width = width;
            image->m_sourceRequest.pixels.height = height;
            image->m_pendingResolve = false;
            return image;
        }

        static void setDefaultTextureLoader(UIImageTextureLoader loader);
        [[nodiscard]] static const UIImageTextureLoader &
        getDefaultTextureLoader();

        void clearImage();
        void setTexture(
            const std::shared_ptr<Core::Renderer::ITexture> &texture);
        void setTextureHandle(Core::Renderer::TextureHandle texture,
                              const glm::vec2 &sourceSize);
        void setSubTexture(
            const std::shared_ptr<Core::Renderer::ITexture> &texture,
            const Core::Renderer::SubTexture &subTexture);
        void setSubTexture(Core::Renderer::TextureHandle texture,
                           const Core::Renderer::SubTexture &subTexture);
        void setSourceFile(const std::string &path);
        bool setPixelData(std::span<const uint8_t> rgba8,
                          uint32_t width,
                          uint32_t height);
        bool setPixelData(std::vector<uint8_t> rgba8,
                          uint32_t width,
                          uint32_t height);
        bool resolveTexture();

        void setTextureLoader(UIImageTextureLoader loader);

        MAKE_GETTER(std::shared_ptr<Core::Renderer::ITexture>,
                    Texture,
                    m_texture)
        MAKE_GETTER(Core::Renderer::TextureHandle,
                    TextureHandle,
                    m_textureHandle)
        MAKE_GETTER(UIImageSourceRequest, SourceRequest, m_sourceRequest)
        MAKE_GETTER_SETTER_WC(glm::vec2, ImageSize, m_imageSize, makeUIDirty)
        MAKE_GETTER_SETTER_WC(UIImageFit, Fit, m_fit, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec4, UVRect, m_uvRect, makeUIDirty)
        MAKE_GETTER_SETTER_WC(glm::vec4,
                              CornerRadius,
                              m_cornerRadius,
                              makeUIDirty)
        MAKE_GETTER_SETTER(Core::Renderer::Color, TintColor, m_tintColor)
        MAKE_GETTER_SETTER(bool, DrawBackground, m_drawBackground)
        MAKE_GETTER_SETTER(bool, DrawBorder, m_drawBorder)
        MAKE_GETTER_SETTER(bool, DrawPlaceholder, m_drawPlaceholder)
        MAKE_GETTER_SETTER(bool, Interactive, m_interactive)
        MAKE_GETTER_SETTER(UIImageClickCallback,
                           ClickCallback,
                           m_clickCallback)

        void draw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        Core::Viewport::SceneCursor getCursor() const override;

      private:
        struct DrawImageFrame {
            glm::vec2 position{0.f};
            glm::vec2 size{0.f};
            glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f};
        };

        [[nodiscard]] glm::vec2 resolveLayoutSize() const;
        [[nodiscard]] glm::vec2 resolveSourceSize() const;
        [[nodiscard]] DrawImageFrame resolveDrawFrame() const;
        [[nodiscard]] PickingId imagePickingId() const;
        [[nodiscard]] bool hasDrawableTexture() const;

        void assignTexture(
            const std::shared_ptr<Core::Renderer::ITexture> &texture,
            const glm::vec4 &uvRect,
            const glm::vec2 &sourceSize);
        void drawBackground(SceneDrawContext &state,
                            const PickingId &id,
                            bool forcePlaceholderBackground) const;
        void drawPlaceholder(SceneDrawContext &state, const PickingId &id) const;
        void markSourcePending();

        std::shared_ptr<Core::Renderer::ITexture> m_texture = nullptr;
        Core::Renderer::TextureHandle m_textureHandle = 0;
        glm::vec2 m_sourceSize{0.f};
        glm::vec2 m_imageSize{64.f, 64.f};
        glm::vec4 m_uvRect{0.f, 0.f, 1.f, 1.f};
        glm::vec4 m_cornerRadius{4.f};
        UIImageFit m_fit = UIImageFit::Contain;
        Core::Renderer::Color m_tintColor{1.f, 1.f, 1.f, 1.f};
        bool m_drawBackground = false;
        bool m_drawBorder = false;
        bool m_drawPlaceholder = true;
        bool m_interactive = false;
        UIImageTextureLoader m_textureLoader = nullptr;
        UIImageClickCallback m_clickCallback = nullptr;
        UIImageSourceRequest m_sourceRequest{};
        bool m_pendingResolve = false;
    };

} // namespace Bess::Canvas::UI
