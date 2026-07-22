#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "common/bess_api.h"

#include <memory>

namespace Bess::Wgpu {
    class WgpuRenderer2D;
    class WgpuTexture;

    class BESS_API WgpuRenderTarget2D final
        : public Core::Renderer::IRenderTarget2D {
      public:
        WgpuRenderTarget2D(
            const std::shared_ptr<WgpuRenderer2D> &renderer,
            const Core::Renderer::RenderTarget2DCreateInfo &createInfo);
        ~WgpuRenderTarget2D() override;

        WgpuRenderTarget2D(const WgpuRenderTarget2D &) = delete;
        WgpuRenderTarget2D &operator=(const WgpuRenderTarget2D &) = delete;
        WgpuRenderTarget2D(WgpuRenderTarget2D &&) = delete;
        WgpuRenderTarget2D &operator=(WgpuRenderTarget2D &&) = delete;

        void destroy() override;
        void resize(const Core::Renderer::Renderer2DExtent &extent) override;
        void beginFrame(
            const Core::Renderer::RenderTarget2DFrameInfo &frameInfo) override;
        void endFrame() override;

        [[nodiscard]] Core::Renderer::Renderer2DExtent
        getExtent() const noexcept override;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getColorTexture() const override;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getPickingTexture() const override;
        [[nodiscard]] PickingId readPickingId(uint32_t x, uint32_t y) override;

      private:
        void replaceAttachments(const Core::Renderer::Renderer2DExtent &extent);

        std::weak_ptr<WgpuRenderer2D> m_renderer;
        std::shared_ptr<WgpuTexture> m_colorTexture;
        std::shared_ptr<WgpuTexture> m_pickingTexture;
        Core::Renderer::Renderer2DExtent m_extent;
        Core::Renderer::Renderer2DTargetFormat m_targetFormat;
        Core::Renderer::Renderer2DTargetFormat m_pickingFormat;
        bool m_usesSurface = false;
        bool m_frameStarted = false;
        bool m_destroyed = false;
    };
} // namespace Bess::Wgpu
