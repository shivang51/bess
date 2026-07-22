#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "common/bess_api.h"

#include <functional>
#include <memory>
#include <optional>

namespace Bess::UI {

    class RenderView;

    struct RenderSurfaceOptions {
        Core::Renderer::Renderer2DExtent initialExtent{1, 1};
        // Empty formats inherit the initialized renderer's formats. This is
        // the portable default because render targets reuse renderer-owned
        // pipelines whose attachment formats must match.
        std::optional<Core::Renderer::Renderer2DTargetFormat> colorFormat;
        std::optional<Core::Renderer::Renderer2DTargetFormat> pickingFormat;
    };

    // Owns one offscreen renderer target and its attachments. The renderer is
    // supplied lazily by UITarget's render-preparation phase, keeping widget
    // composition independent of a backend/device. Calls are intentionally
    // single-thread-affine, matching IRenderer2D's frame contract. At most one
    // mounted RenderView may produce a surface; any number of passive Image
    // consumers may present its current color attachment.
    class BESS_API RenderSurface final {
      public:
        using RenderCallback = std::function<void(
            Core::Renderer::IRenderer2D &, Core::Renderer::IRenderTarget2D &)>;

        explicit RenderSurface(RenderSurfaceOptions options = {});
        ~RenderSurface();

        RenderSurface(const RenderSurface &) = delete;
        RenderSurface &operator=(const RenderSurface &) = delete;
        RenderSurface(RenderSurface &&) = delete;
        RenderSurface &operator=(RenderSurface &&) = delete;

        // Idempotent for the same renderer. Switching a live surface between
        // renderers is rejected; destroy it first so backend handles can be
        // released by their owning device.
        void initialize(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer);
        void destroy();

        [[nodiscard]] bool isInitialized() const noexcept;
        [[nodiscard]] bool isRendering() const noexcept;
        [[nodiscard]] const RenderSurfaceOptions &options() const noexcept;
        [[nodiscard]] Core::Renderer::Renderer2DExtent extent() const noexcept;
        // Returns true only when attachment dimensions changed.
        bool resize(Core::Renderer::Renderer2DExtent extent);

        // Owns beginFrame/endFrame and guarantees endFrame after a successful
        // begin, including when application rendering throws. The callback
        // issues draw commands only; it must not nest another frame on this
        // renderer or resize/destroy this surface. If both rendering and
        // endFrame fail, the original application exception is preserved.
        void render(const Core::Renderer::RenderTarget2DFrameInfo &frameInfo,
                    const RenderCallback &callback = {});

        [[nodiscard]] std::shared_ptr<Core::Renderer::IRenderer2D>
        renderer() const noexcept;
        // Attachment getters return snapshots. resize() may replace the
        // texture objects, so retained presenters must reacquire them (for
        // example through UIComposer::dynamicImage). Never sample either
        // attachment while this surface is the active render destination;
        // use a separate or ping-pong surface for feedback effects.
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        colorTexture() const;
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        pickingTexture() const;
        [[nodiscard]] PickingId readPickingId(uint32_t x, uint32_t y);

      private:
        friend class RenderView;
        void attachProducer(const void *producer);
        void detachProducer(const void *producer) noexcept;

        RenderSurfaceOptions m_options;
        std::shared_ptr<Core::Renderer::IRenderer2D> m_renderer;
        std::shared_ptr<Core::Renderer::IRenderTarget2D> m_target;
        const void *m_producer = nullptr;
        bool m_rendering = false;
    };

} // namespace Bess::UI
