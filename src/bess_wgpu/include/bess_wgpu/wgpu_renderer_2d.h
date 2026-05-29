#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include <cstddef>
#include <memory>
#include <string>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu {

    class WgpuRenderer2D final : public Core::Renderer::IRenderer2D {
      public:
        WgpuRenderer2D();
        ~WgpuRenderer2D() override;

        WgpuRenderer2D(const WgpuRenderer2D &) = delete;
        WgpuRenderer2D &operator=(const WgpuRenderer2D &) = delete;
        WgpuRenderer2D(WgpuRenderer2D &&) = delete;
        WgpuRenderer2D &operator=(WgpuRenderer2D &&) = delete;

        void
        init(const Core::Renderer::Renderer2DCreateInfo &createInfo) override;
        void destroy() override;

        void resize(const Core::Renderer::Renderer2DExtent &extent) override;

        void beginFrame(
            const Core::Renderer::Renderer2DFrameInfo &frameInfo) override;
        void endFrame() override;

        void clear(const Core::Renderer::Color &color) override;
        void saveTargetToFile(const std::string &path) override;
        [[nodiscard]] Core::Renderer::Renderer2DStats
        getStats() const noexcept override;

        Core::Renderer::TextureHandle
        createTexture(Core::Renderer::ITexture &texture) override;
        Core::Renderer::TextureHandle
        createTexture(const Core::Renderer::TextureCreateInfo &createInfo);
        void destroyTexture(Core::Renderer::TextureHandle texture) override;

        void drawQuad(const Core::Renderer::QuadProps &props) override;
        void drawRoundedQuad(
            const Core::Renderer::QuadProps &props,
            const Core::Renderer::RoundedBorderProps &roundedProps) override;

        void
        drawImGui(const std::function<void(void *)> &imguiRenderFn) override;

        void drawToWindow(const std::function<void(void *)> &renderFn) override;

        [[nodiscard]] wgpu::Device getDevice() const;
        [[nodiscard]] wgpu::Queue getQueue() const;
        [[nodiscard]] wgpu::TextureView getCurrentTargetView() const;
        [[nodiscard]] wgpu::TextureFormat getTargetFormat() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Bess::Wgpu
