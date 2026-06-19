#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_texture.h"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu {

    struct TextureResource {
        wgpu::Texture texture;
        wgpu::TextureView view;
        wgpu::BindGroup bindGroup;
        Core::Renderer::TextureHandle handle = 0;
        uint32_t width = 1;
        uint32_t height = 1;
        wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm;
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
        void saveTextureToFile(Core::Renderer::TextureHandle texture,
                               const std::string &path);
        [[nodiscard]] Core::Renderer::Renderer2DStats
        getStats() const noexcept override;
        using Core::Renderer::IRenderer2D::readTexture;
        [[nodiscard]] Core::Renderer::TextureReadbackResult readTexture(
            const Core::Renderer::TextureReadbackRegion &region) override;
        using Core::Renderer::IRenderer2D::requestPickingId;
        void requestPickingIds(
            const Core::Renderer::TextureReadbackRegion &region) override;
        [[nodiscard]] bool tryGetPickingIds(
            Core::Renderer::PickingReadbackResult &result) override;
        [[nodiscard]] bool isPickingReadbackPending() const noexcept override;

        void unregisterTexture(Core::Renderer::TextureHandle texture);

        void registerTexture(const TextureResource &texture);

        void drawQuad(const Core::Renderer::QuadProps &props) override;
        [[nodiscard]] Core::Renderer::CustomQuadShaderHandle
        createCustomQuadShader(
            const Core::Renderer::CustomQuadShaderDesc &desc) override;
        void destroyCustomQuadShader(
            Core::Renderer::CustomQuadShaderHandle shader) override;
        void
        drawCustomQuad(const Core::Renderer::CustomQuadProps &props) override;

        void
        drawCustomQuad(const Core::Renderer::QuadProps &quad,
                       Core::Renderer::CustomQuadShaderHandle shader,
                       std::array<glm::vec4, 4> data = {},
                       Core::Renderer::CustomQuadTransformMode transformMode =
                           Core::Renderer::CustomQuadTransformMode::Camera);

        void drawCircle(const Core::Renderer::CircleProps &props) override;

        void drawLine(const Core::Renderer::LineProps &props) override;

        void drawFont(std::string_view text,
                      const Core::Renderer::FontProps &props = {}) override;
        [[nodiscard]] glm::vec2
        measureText(std::string_view text,
                    const Core::Renderer::FontProps &props = {}) override;
        [[nodiscard]] float
        textCenterOffsetY(std::string_view text,
                          const Core::Renderer::FontProps &props = {}) override;

        void drawPath(std::span<const Core::Renderer::PathCommand> commands,
                      const Core::Renderer::PathProps &props = {}) override;
        void drawPath(const Core::Renderer::Path2D &path,
                      const Core::Renderer::PathProps &props = {}) override;
        using Core::Renderer::IRenderer2D::drawPath;

        void beginPath(const Core::Renderer::PathProps &props = {}) override;
        void pathMoveTo(const glm::vec2 &pos) override;
        void pathLineTo(
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathLineTo(const glm::vec2 &pos, float strokeWidth) {
            pathLineTo(
                pos, Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathLineTo(const glm::vec2 &pos, float strokeWidth, PickingId id) {
            pathLineTo(pos,
                       Core::Renderer::PathCommandStroke::withWidthAndId(
                           strokeWidth, id));
        }
        void pathQuadTo(
            const glm::vec2 &control,
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathQuadTo(const glm::vec2 &control,
                        const glm::vec2 &pos,
                        float strokeWidth) {
            pathQuadTo(
                control,
                pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathQuadTo(const glm::vec2 &control,
                        const glm::vec2 &pos,
                        float strokeWidth,
                        PickingId id) {
            pathQuadTo(control,
                       pos,
                       Core::Renderer::PathCommandStroke::withWidthAndId(
                           strokeWidth, id));
        }

        void pathCubicTo(
            const glm::vec2 &control1,
            const glm::vec2 &control2,
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;

        void pathCubicTo(const glm::vec2 &control1,
                         const glm::vec2 &control2,
                         const glm::vec2 &pos,
                         float strokeWidth);

        void pathCubicTo(const glm::vec2 &control1,
                         const glm::vec2 &control2,
                         const glm::vec2 &pos,
                         float strokeWidth,
                         PickingId id);
        void pathClose(
            const Core::Renderer::PathCommandStroke &stroke = {}) override;

        void pathClose(float strokeWidth);

        void pathClose(float strokeWidth, PickingId id);

        void endPath() override;

        void drawToWindow(const std::shared_ptr<Window> &window,
                          const std::function<void(void *)> &renderFn) override;

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
