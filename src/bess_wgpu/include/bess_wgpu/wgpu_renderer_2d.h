#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_texture.h"
#include <memory>
#include <string>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu {

    struct TextureResource {
        wgpu::Texture texture;
        wgpu::TextureView view;
        wgpu::BindGroup bindGroup;
        Core::Renderer::TextureHandle handle = 0;
        uint32_t width = 1;
        uint32_t height = 1;
    };

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

        void unregisterTexture(Core::Renderer::TextureHandle texture);

        void registerTexture(const TextureResource &texture);

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
        [[nodiscard]] Core::Renderer::Renderer2DTargetFormat
        getTargetFormatType() const;
        [[nodiscard]] wgpu::TextureFormat getTargetFormat() const;
        [[nodiscard]] wgpu::TextureFormat getSurfaceFormat() const;

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Bess::Wgpu
