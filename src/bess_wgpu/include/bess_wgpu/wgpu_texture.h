#pragma once

#include "bess_core/renderer/texture.h"
#include "webgpu/webgpu_cpp.h"

namespace Bess::Wgpu {
    struct TextureResource;

    class WgpuRenderer2D;

    class WgpuTexture final : public Core::Renderer::ITexture {
      public:
        static void
        setRenderer(const std::shared_ptr<WgpuRenderer2D> &renderer) noexcept;

        static std::shared_ptr<WgpuTexture>
        fromPixels(const uint8_t *pixels, uint32_t width, uint32_t height);

        explicit WgpuTexture() = default;
        WgpuTexture(const std::string &path);
        WgpuTexture(const Core::Renderer::TextureCreateInfo &createInfo);
        ~WgpuTexture() override;

        WgpuTexture(const WgpuTexture &) = delete;
        WgpuTexture &operator=(const WgpuTexture &) = delete;
        WgpuTexture(WgpuTexture &&) = delete;
        WgpuTexture &operator=(WgpuTexture &&) = delete;

        void init() override;
        void destroy() override;
        void *getView() const override;

        wgpu::TextureView getTextureView() const;

        TextureResource getResource() const;

      private:
        void initTexture();
        // If path is empty, creates a render target
        void initRenderTarget();

        void createTextureFromPixels(const uint8_t *pixels, uint32_t width,
                                     uint32_t height);

      private:
        static std::shared_ptr<WgpuRenderer2D> s_renderer;
        wgpu::Texture m_wgpuHandle = nullptr;
        wgpu::TextureView m_textureView = nullptr;
        bool m_isRenderTarget = false;
    };

} // namespace Bess::Wgpu
