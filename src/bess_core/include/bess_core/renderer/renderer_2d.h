#pragma once
#include "bess_core/renderer/texture.h"
#include "bess_core/renderer/renderer_types.h"
#include <cstdint>
#include <string>

namespace Bess::Core::Renderer {
    enum class Renderer2DTargetFormat : uint8_t {
        RGBA8Unorm,
        BGRA8Unorm,
        RGBA16Float
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
        uint32_t maxQuadCapacity = 65536;
    };

    struct Renderer2DCreateInfo {
        Renderer2DExtent extent;
        Renderer2DTargetFormat targetFormat = Renderer2DTargetFormat::BGRA8Unorm;
        Renderer2DNativeSurface surface;
        Renderer2DBatchConfig batching;
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

        virtual TextureHandle createTexture(const ITexture &texture) = 0;
        virtual void destroyTexture(TextureHandle texture) = 0;

        virtual void drawQuad(const QuadProps &props) = 0;

        virtual void
        drawRoundedQuad(const QuadProps &props,
                        const RoundedBorderProps &roundedProps) = 0;
    };

} // namespace Bess::Core::Renderer
