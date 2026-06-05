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

    using CustomQuadShaderHandle = uint32_t;

    struct CustomQuadShaderDesc {
        std::string label;
        // Appended after the renderer's WGSL prelude. Define:
        // fn <fragmentEntryPoint>(in: CustomQuadFragmentInput) -> vec4f
        //
        // CustomQuadFragmentInput contains:
        // frag_coord, uv, local_uv, local_pos, size, color, data0..data3.
        // uv respects QuadProps::uvRect; local_uv is always 0..1.
        std::string fragmentSource;
        std::string fragmentEntryPoint = "custom_quad_fragment";
    };

    struct CustomQuadProps {
        Core::Renderer::QuadProps quad;
        CustomQuadShaderHandle shader = 0;
        std::array<glm::vec4, 4> data{};
    };

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
        [[nodiscard]] CustomQuadShaderHandle
        createCustomQuadShader(const CustomQuadShaderDesc &desc);
        void destroyCustomQuadShader(CustomQuadShaderHandle shader);
        void drawCustomQuad(const CustomQuadProps &props);
        void drawCustomQuad(const Core::Renderer::QuadProps &quad,
                            CustomQuadShaderHandle shader,
                            std::array<glm::vec4, 4> data = {});
        void drawRoundedQuad(
            const Core::Renderer::QuadProps &props,
            const Core::Renderer::RoundedBorderProps &roundedProps) override;

        void drawCircle(const Core::Renderer::CircleProps &props) override;

        void drawLine(const Core::Renderer::LineProps &props) override;

        void drawFont(std::string_view text,
                      const Core::Renderer::FontProps &props = {}) override;

        void drawPath(std::span<const Core::Renderer::PathCommand> commands,
                      const Core::Renderer::PathProps &props = {}) override;
        void drawPath(const Core::Renderer::Path2D &path,
                      const Core::Renderer::PathProps &props = {}) override;

        void beginPath(const Core::Renderer::PathProps &props = {}) override;
        void pathMoveTo(const glm::vec2 &pos) override;
        void pathLineTo(
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathLineTo(const glm::vec2 &pos, float strokeWidth) {
            pathLineTo(
                pos, Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathQuadTo(
            const glm::vec2 &control, const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathQuadTo(const glm::vec2 &control, const glm::vec2 &pos,
                        float strokeWidth) {
            pathQuadTo(
                control, pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathQuadraticTo(
            const glm::vec2 &control, const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathQuadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                             float strokeWidth) {
            pathQuadraticTo(
                control, pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathCubicTo(
            const glm::vec2 &control1, const glm::vec2 &control2,
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathCubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                         const glm::vec2 &pos, float strokeWidth) {
            pathCubicTo(
                control1, control2, pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathCubicBezierTo(
            const glm::vec2 &control1, const glm::vec2 &control2,
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathCubicBezierTo(const glm::vec2 &control1,
                               const glm::vec2 &control2, const glm::vec2 &pos,
                               float strokeWidth) {
            pathCubicBezierTo(
                control1, control2, pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathBezierCurveTo(
            const glm::vec2 &control1, const glm::vec2 &control2,
            const glm::vec2 &pos,
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathBezierCurveTo(const glm::vec2 &control1,
                               const glm::vec2 &control2, const glm::vec2 &pos,
                               float strokeWidth) {
            pathBezierCurveTo(
                control1, control2, pos,
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void pathClose(
            const Core::Renderer::PathCommandStroke &stroke = {}) override;
        void pathClose(float strokeWidth) {
            pathClose(
                Core::Renderer::PathCommandStroke::withWidth(strokeWidth));
        }
        void endPath() override;

        void
        drawImGui(const std::function<void(void *)> &imguiRenderFn) override;

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
