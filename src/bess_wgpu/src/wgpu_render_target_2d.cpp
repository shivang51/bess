#include "bess_wgpu/wgpu_render_target_2d.h"

#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"

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
        recreateAttachments();
    }

    WgpuRenderTarget2D::~WgpuRenderTarget2D() {
        destroy();
    }

    void WgpuRenderTarget2D::destroy() {
        if (m_destroyed) {
            return;
        }

        if (m_frameStarted) {
            if (const auto renderer = m_renderer.lock()) {
                renderer->endFrame();
            }
            m_frameStarted = false;
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

        m_extent = extent;
        recreateAttachments();
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

        renderer->endFrame();
        m_frameStarted = false;
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

    void WgpuRenderTarget2D::recreateAttachments() {
        const auto renderer = m_renderer.lock();
        if (renderer == nullptr) {
            throw std::runtime_error("Render target renderer was destroyed");
        }

        if (m_colorTexture != nullptr) {
            m_colorTexture->destroy();
            m_colorTexture.reset();
        }
        if (m_pickingTexture != nullptr) {
            m_pickingTexture->destroy();
            m_pickingTexture.reset();
        }

        if (m_extent.width == 0 || m_extent.height == 0) {
            return;
        }

        const glm::vec2 size{static_cast<float>(m_extent.width),
                             static_cast<float>(m_extent.height)};

        if (!m_usesSurface) {
            m_colorTexture = std::make_shared<WgpuTexture>(
                Core::Renderer::TextureCreateInfo{.format = m_targetFormat});
            m_colorTexture->setRenderer(renderer);
            m_colorTexture->setSize(size);
            m_colorTexture->init();
        }

        if (m_pickingFormat != Core::Renderer::Renderer2DTargetFormat::None) {
            m_pickingTexture = std::make_shared<WgpuTexture>(
                Core::Renderer::TextureCreateInfo{.format = m_pickingFormat});
            m_pickingTexture->setRenderer(renderer);
            m_pickingTexture->setSize(size);
            m_pickingTexture->init();
        }
    }
} // namespace Bess::Wgpu
