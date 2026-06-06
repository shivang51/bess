#pragma once
#include "bess_core/renderer/renderer_path.h"
#include "bess_core/renderer/renderer_types.h"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Bess {
    class Window;
}

namespace Bess::Core::Renderer {
    enum class Renderer2DTargetFormat : uint8_t {
        None,
        RGBA8Unorm,
        BGRA8Unorm,
        RGBA16Float,
        RG32Uint
    };

    struct Renderer2DExtent {
        uint32_t width = 1;
        uint32_t height = 1;
    };

    enum class Renderer2DNativeSurfaceType : uint8_t {
        None,
        BackendOwned,
        PlatformHandle
    };

    struct Renderer2DNativeSurface {
        Renderer2DNativeSurfaceType type = Renderer2DNativeSurfaceType::None;
        void *handle = nullptr;
    };

    struct Renderer2DBatchConfig {
        uint32_t initialQuadCapacity = 1024;
        uint32_t maxQuadCapacity =
            524288; // Assuming atleast 128MB of GPU memory will be there and
                    // 256 bytes per quad instance
    };

    struct Renderer2DCreateInfo {
        Renderer2DExtent extent;
        Renderer2DTargetFormat targetFormat =
            Renderer2DTargetFormat::BGRA8Unorm;
        Renderer2DTargetFormat pickingFormat = Renderer2DTargetFormat::None;
        Renderer2DNativeSurface surface;
        Renderer2DBatchConfig batching;
        // Empty uses assets/fonts/Roboto/Roboto-Regular.ttf.
        std::string fontFile;
        bool enableValidation = true;
    };

    struct Renderer2DStats {
        uint32_t quadCount = 0;
        uint32_t drawCallCount = 0;
        uint64_t uploadedBytes = 0;
    };

    struct Renderer2DFrameInfo {
        Renderer2DExtent extent{0, 0};
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        TextureHandle targetTexture = 0;
        TextureHandle pickingTexture = 0;
        float *cameraTransform = nullptr;
    };

    struct TextureReadbackRegion {
        TextureHandle texture = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 1;
        uint32_t height = 1;
    };

    struct TextureReadbackResult {
        Renderer2DTargetFormat format = Renderer2DTargetFormat::None;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bytesPerPixel = 0;
        std::vector<uint8_t> pixels;

        [[nodiscard]] bool empty() const noexcept { return pixels.empty(); }
    };

    struct PickingReadbackResult {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        std::vector<PickingId> ids;

        [[nodiscard]] bool empty() const noexcept { return ids.empty(); }
        [[nodiscard]] PickingId firstOrInvalid() const noexcept {
            return ids.empty() ? PickingId::invalid() : ids.front();
        }
    };

    using CustomQuadShaderHandle = uint32_t;

    enum class CustomQuadTransformMode : uint8_t {
        // QuadProps position/size are scene/world units and frame.camera_transform
        // is applied in the vertex shader.
        Camera,
        // QuadProps position/size are render-target pixels with origin at the
        // target center; frame.camera_transform is not applied.
        Screen,
    };

    struct CustomQuadShaderDesc {
        std::string label;
        // Appended after the renderer's WGSL prelude. Define:
        // fn <fragmentEntryPoint>(in: CustomQuadFragmentInput) -> vec4f
        //
        // CustomQuadFragmentInput contains:
        // frag_coord, uv, local_uv, local_pos, size, color, data0..data3,
        // viewport, camera_transform, camera_zoom, camera_zoom_xy.
        // uv respects QuadProps::uvRect; local_uv is always 0..1.
        //
        // The WGSL frame uniform is also in scope as `frame`, with:
        // frame.viewport and frame.camera_transform.
        std::string fragmentSource;
        std::string fragmentEntryPoint = "custom_quad_fragment";
    };

    struct CustomQuadProps {
        QuadProps quad;
        CustomQuadShaderHandle shader = 0;
        std::array<glm::vec4, 4> data{};
        CustomQuadTransformMode transformMode = CustomQuadTransformMode::Camera;
    };

    class IRenderer2D {
      public:
        virtual ~IRenderer2D();

        virtual void init(const Renderer2DCreateInfo &createInfo) = 0;
        virtual void destroy() = 0;

        virtual void resize(const Renderer2DExtent &extent) = 0;

        virtual void beginFrame(const Renderer2DFrameInfo &frameInfo) = 0;
        virtual void endFrame() = 0;

        virtual void clear(const Color &color) = 0;
        virtual void saveTargetToFile(const std::string &path) = 0;
        [[nodiscard]] virtual Renderer2DStats getStats() const noexcept = 0;

        [[nodiscard]] virtual TextureReadbackResult
        readTexture(const TextureReadbackRegion &region) = 0;
        [[nodiscard]] TextureReadbackResult readTexture(TextureHandle texture,
                                                        uint32_t x, uint32_t y,
                                                        uint32_t width = 1,
                                                        uint32_t height = 1);
        [[nodiscard]] PickingId readPickingId(TextureHandle texture,
                                              uint32_t x, uint32_t y);
        [[nodiscard]] std::vector<PickingId>
        readPickingIds(TextureHandle texture, uint32_t x, uint32_t y,
                       uint32_t width, uint32_t height);
        virtual void requestPickingIds(const TextureReadbackRegion &region) = 0;
        void requestPickingId(TextureHandle texture, uint32_t x, uint32_t y);
        [[nodiscard]] virtual bool
        tryGetPickingIds(PickingReadbackResult &result) = 0;
        [[nodiscard]] virtual bool
        isPickingReadbackPending() const noexcept = 0;

        virtual void drawQuad(const QuadProps &props) = 0;

        [[nodiscard]] virtual CustomQuadShaderHandle
        createCustomQuadShader(const CustomQuadShaderDesc &desc) = 0;
        virtual void destroyCustomQuadShader(CustomQuadShaderHandle shader) = 0;
        virtual void drawCustomQuad(const CustomQuadProps &props) = 0;
        void drawCustomQuad(const QuadProps &quad,
                            CustomQuadShaderHandle shader,
                            std::array<glm::vec4, 4> data = {},
                            CustomQuadTransformMode transformMode =
                                CustomQuadTransformMode::Camera) {
            drawCustomQuad(
                CustomQuadProps{.quad = quad,
                                .shader = shader,
                                .data = data,
                                .transformMode = transformMode});
        }

        virtual void
        drawRoundedQuad(const QuadProps &props,
                        const RoundedBorderProps &roundedProps) = 0;

        virtual void drawCircle(const CircleProps &props) = 0;

        virtual void drawLine(const LineProps &props) = 0;

        virtual void drawFont(std::string_view text,
                              const FontProps &props = {}) = 0;

        virtual void drawPath(std::span<const PathCommand> commands,
                              const PathProps &props = {}) = 0;
        virtual void drawPath(const Path2D &path, const PathProps &props = {}) {
            drawPath(path.commands(), props);
        }

        virtual void beginPath(const PathProps &props = {}) = 0;
        virtual void pathMoveTo(const glm::vec2 &pos) = 0;
        virtual void pathLineTo(const glm::vec2 &pos,
                                const PathCommandStroke &stroke = {}) = 0;
        void pathLineTo(const glm::vec2 &pos, float strokeWidth) {
            pathLineTo(pos, PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void pathQuadTo(const glm::vec2 &control, const glm::vec2 &pos,
                                const PathCommandStroke &stroke = {}) = 0;
        void pathQuadTo(const glm::vec2 &control, const glm::vec2 &pos,
                        float strokeWidth) {
            pathQuadTo(control, pos, PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void pathQuadraticTo(const glm::vec2 &control,
                                     const glm::vec2 &pos,
                                     const PathCommandStroke &stroke = {}) = 0;
        void pathQuadraticTo(const glm::vec2 &control, const glm::vec2 &pos,
                             float strokeWidth) {
            pathQuadraticTo(control, pos,
                            PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void pathCubicTo(const glm::vec2 &control1,
                                 const glm::vec2 &control2,
                                 const glm::vec2 &pos,
                                 const PathCommandStroke &stroke = {}) = 0;
        void pathCubicTo(const glm::vec2 &control1, const glm::vec2 &control2,
                         const glm::vec2 &pos, float strokeWidth) {
            pathCubicTo(control1, control2, pos,
                        PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void
        pathCubicBezierTo(const glm::vec2 &control1, const glm::vec2 &control2,
                          const glm::vec2 &pos,
                          const PathCommandStroke &stroke = {}) = 0;
        void pathCubicBezierTo(const glm::vec2 &control1,
                               const glm::vec2 &control2, const glm::vec2 &pos,
                               float strokeWidth) {
            pathCubicBezierTo(control1, control2, pos,
                              PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void
        pathBezierCurveTo(const glm::vec2 &control1, const glm::vec2 &control2,
                          const glm::vec2 &pos,
                          const PathCommandStroke &stroke = {}) = 0;
        void pathBezierCurveTo(const glm::vec2 &control1,
                               const glm::vec2 &control2, const glm::vec2 &pos,
                               float strokeWidth) {
            pathBezierCurveTo(control1, control2, pos,
                              PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void pathClose(const PathCommandStroke &stroke = {}) = 0;
        void pathClose(float strokeWidth) {
            pathClose(PathCommandStroke::withWidth(strokeWidth));
        }
        virtual void endPath() = 0;

        virtual void
        drawImGui(const std::function<void(void *)> &imguiRenderFn) {}

        virtual void drawToWindow(const std::shared_ptr<Window> &window,
                                  const std::function<void(void *)> &renderFn) {
        }
    };

} // namespace Bess::Core::Renderer
