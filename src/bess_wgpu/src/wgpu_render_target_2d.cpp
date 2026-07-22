#include "bess_wgpu/wgpu_render_target_2d.h"

#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"

#include <exception>
#include <stdexcept>

namespace Bess::Wgpu {
    WgpuRenderTarget2D::WgpuRenderTarget2D(
        const std::shared_ptr<WgpuRenderer2D> &renderer,
        const Core::Renderer::RenderTarget2DCreateInfo &createInfo)
        : m_renderer(renderer),
          m_extent(createInfo.extent),
          m_targetFormat(createInfo.targetFormat),
          m_pickingFormat(createInfo.pickingFormat),
          m_usesSurface(createInfo.surface.type !=
                        Core::Renderer::Renderer2DNativeSurfaceType::None) {
        BESS_ASSERT(renderer != nullptr,
                    "A render target requires an initialized renderer");
        replaceAttachments(createInfo.extent);
    }

    WgpuRenderTarget2D::~WgpuRenderTarget2D() {
        // Destructors are a last-resort cleanup path and must never terminate
        // the process because a pending GPU frame failed to encode.
        try {
            destroy();
        } catch (...) {
        }
    }

    void WgpuRenderTarget2D::destroy() {
        if (m_destroyed) {
            return;
        }

        std::exception_ptr frameFailure;
        if (m_frameStarted) {
            m_frameStarted = false;
            if (const auto renderer = m_renderer.lock()) {
                try {
                    renderer->endFrame();
                } catch (...) {
                    frameFailure = std::current_exception();
                }
            }
        }

        if (m_pickingTexture != nullptr) {
            m_pickingTexture->destroy();
            m_pickingTexture.reset();
        }
        if (m_colorTexture != nullptr) {
            m_colorTexture->destroy();
            m_colorTexture.reset();
        }

        m_renderer.reset();
        m_destroyed = true;

        // Explicit destroy() callers still receive the backend failure, but
        // only after the target has reached a fully destroyed state.
        if (frameFailure) {
            std::rethrow_exception(frameFailure);
        }
    }

    void
    WgpuRenderTarget2D::resize(const Core::Renderer::Renderer2DExtent &extent) {
        if (m_destroyed) {
            throw std::runtime_error("Cannot resize a destroyed render target");
        }
        if (m_frameStarted) {
            throw std::runtime_error(
                "Cannot resize a render target during an active frame");
        }
        if (m_extent.width == extent.width &&
            m_extent.height == extent.height) {
            return;
        }

        replaceAttachments(extent);
    }

    void WgpuRenderTarget2D::beginFrame(
        const Core::Renderer::RenderTarget2DFrameInfo &frameInfo) {
        if (m_destroyed) {
            throw std::runtime_error(
                "Cannot begin a frame on a destroyed render target");
        }
        if (m_frameStarted) {
            throw std::runtime_error("Render target frame already started");
        }
        if (m_extent.width == 0 || m_extent.height == 0) {
            throw std::runtime_error(
                "Cannot begin a frame on a zero-sized render target");
        }

        const auto renderer = m_renderer.lock();
        if (renderer == nullptr) {
            throw std::runtime_error("Render target renderer was destroyed");
        }

        renderer->beginFrame({
            .extent = m_extent,
            .clearColor = frameInfo.clearColor,
            .shouldClear = frameInfo.shouldClear,
            .targetTexture =
                m_colorTexture != nullptr ? m_colorTexture->getHandle() : 0,
            .pickingTexture =
                m_pickingTexture != nullptr ? m_pickingTexture->getHandle() : 0,
            .cameraTransform = frameInfo.cameraTransform,
        });
        m_frameStarted = true;
    }

    void WgpuRenderTarget2D::endFrame() {
        if (!m_frameStarted) {
            return;
        }

        const auto renderer = m_renderer.lock();
        if (renderer == nullptr) {
            m_frameStarted = false;
            throw std::runtime_error("Render target renderer was destroyed");
        }

        // Release the wrapper state before invoking backend work. The renderer
        // abandons its own transient frame state if encoding fails, and this
        // target must remain immediately reusable after the exception.
        m_frameStarted = false;
        renderer->endFrame();
    }

    Core::Renderer::Renderer2DExtent
    WgpuRenderTarget2D::getExtent() const noexcept {
        return m_extent;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    WgpuRenderTarget2D::getColorTexture() const {
        return m_colorTexture;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    WgpuRenderTarget2D::getPickingTexture() const {
        return m_pickingTexture;
    }

    PickingId WgpuRenderTarget2D::readPickingId(uint32_t x, uint32_t y) {
        if (m_pickingTexture == nullptr || x >= m_extent.width ||
            y >= m_extent.height) {
            return PickingId::invalid();
        }

        const auto renderer = m_renderer.lock();
        if (renderer == nullptr) {
            return PickingId::invalid();
        }
        return renderer->readPickingId(m_pickingTexture->getHandle(), x, y);
    }

    void WgpuRenderTarget2D::replaceAttachments(
        const Core::Renderer::Renderer2DExtent &extent) {
        const auto renderer = m_renderer.lock();
        if (renderer == nullptr) {
            throw std::runtime_error("Render target renderer was destroyed");
        }

        std::shared_ptr<WgpuTexture> colorTexture;
        std::shared_ptr<WgpuTexture> pickingTexture;

        if (extent.width != 0 && extent.height != 0) {
            const glm::vec2 size{static_cast<float>(extent.width),
                                 static_cast<float>(extent.height)};

            if (!m_usesSurface) {
                colorTexture = std::make_shared<WgpuTexture>(
                    Core::Renderer::TextureCreateInfo{.format =
                                                          m_targetFormat});
                colorTexture->setRenderer(renderer);
                colorTexture->setSize(size);
                colorTexture->init();
            }

            if (m_pickingFormat !=
                Core::Renderer::Renderer2DTargetFormat::None) {
                pickingTexture = std::make_shared<WgpuTexture>(
                    Core::Renderer::TextureCreateInfo{.format =
                                                          m_pickingFormat});
                pickingTexture->setRenderer(renderer);
                pickingTexture->setSize(size);
                pickingTexture->init();
            }
        }

        // Commit only after every required attachment has been initialized and
        // registered. Swaps are noexcept, so allocation/initialization failure
        // leaves the old extent and both old attachments intact.
        m_colorTexture.swap(colorTexture);
        m_pickingTexture.swap(pickingTexture);
        m_extent = extent;
    }
} // namespace Bess::Wgpu
