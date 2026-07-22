#include "render_surface.h"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

namespace Bess::UI {

    RenderSurface::RenderSurface(RenderSurfaceOptions options)
        : m_options(std::move(options)) {
        m_options.initialExtent.width =
            std::max(1U, m_options.initialExtent.width);
        m_options.initialExtent.height =
            std::max(1U, m_options.initialExtent.height);
    }

    RenderSurface::~RenderSurface() {
        try {
            destroy();
        } catch (...) {
            // Resource cleanup is best-effort in a destructor. Explicit
            // destroy() remains available when a caller must observe errors.
        }
    }

    void RenderSurface::initialize(
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        if (renderer == nullptr) {
            throw std::invalid_argument(
                "RenderSurface requires an initialized renderer");
        }
        if (m_target != nullptr) {
            if (m_renderer.get() != renderer.get()) {
                throw std::logic_error(
                    "A live RenderSurface cannot switch renderers");
            }
            return;
        }

        auto target = renderer->createTarget({
            .extent = m_options.initialExtent,
            .targetFormat =
                m_options.colorFormat.value_or(renderer->getTargetFormatType()),
            .pickingFormat = m_options.pickingFormat.value_or(
                renderer->getPickingFormatType()),
            .surface = {},
        });
        if (target == nullptr) {
            throw std::runtime_error(
                "Renderer failed to create an offscreen RenderSurface");
        }
        m_renderer = renderer;
        m_target = std::move(target);
    }

    void RenderSurface::destroy() {
        if (m_rendering) {
            throw std::logic_error(
                "Cannot destroy RenderSurface during an active frame");
        }
        if (m_target != nullptr) {
            m_target->destroy();
            m_target.reset();
        }
        m_renderer.reset();
    }

    bool RenderSurface::isInitialized() const noexcept {
        return m_renderer != nullptr && m_target != nullptr;
    }

    bool RenderSurface::isRendering() const noexcept {
        return m_rendering;
    }

    const RenderSurfaceOptions &RenderSurface::options() const noexcept {
        return m_options;
    }

    Core::Renderer::Renderer2DExtent RenderSurface::extent() const noexcept {
        return m_target != nullptr ? m_target->getExtent()
                                   : m_options.initialExtent;
    }

    bool RenderSurface::resize(Core::Renderer::Renderer2DExtent extent) {
        if (m_target == nullptr) {
            throw std::logic_error(
                "Cannot resize an uninitialized RenderSurface");
        }
        if (m_rendering) {
            throw std::logic_error(
                "Cannot resize RenderSurface during an active frame");
        }
        if (extent.width == 0 || extent.height == 0) {
            throw std::invalid_argument(
                "RenderSurface extent must be non-zero");
        }
        const auto current = m_target->getExtent();
        if (current.width == extent.width && current.height == extent.height) {
            return false;
        }
        m_target->resize(extent);
        return true;
    }

    void RenderSurface::render(
        const Core::Renderer::RenderTarget2DFrameInfo &frameInfo,
        const RenderCallback &callback) {
        if (!isInitialized()) {
            throw std::logic_error(
                "Cannot render an uninitialized RenderSurface");
        }
        if (m_rendering) {
            throw std::logic_error("RenderSurface frame is already active");
        }
        const auto size = m_target->getExtent();
        if (size.width == 0 || size.height == 0) {
            throw std::logic_error("Cannot render a zero-sized RenderSurface");
        }

        m_target->beginFrame(frameInfo);
        m_rendering = true;
        std::exception_ptr failure;
        try {
            if (callback) {
                callback(*m_renderer, *m_target);
            }
        } catch (...) {
            failure = std::current_exception();
        }
        try {
            m_target->endFrame();
        } catch (...) {
            if (failure == nullptr) {
                failure = std::current_exception();
            }
        }
        m_rendering = false;
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    std::shared_ptr<Core::Renderer::IRenderer2D>
    RenderSurface::renderer() const noexcept {
        return m_renderer;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    RenderSurface::colorTexture() const {
        return m_target != nullptr ? m_target->getColorTexture() : nullptr;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    RenderSurface::pickingTexture() const {
        return m_target != nullptr ? m_target->getPickingTexture() : nullptr;
    }

    PickingId RenderSurface::readPickingId(uint32_t x, uint32_t y) {
        if (m_target == nullptr) {
            return PickingId::invalid();
        }
        const auto size = m_target->getExtent();
        return x < size.width && y < size.height ? m_target->readPickingId(x, y)
                                                 : PickingId::invalid();
    }

    void RenderSurface::attachProducer(const void *producer) {
        if (producer == nullptr) {
            throw std::invalid_argument(
                "RenderSurface producer identity cannot be null");
        }
        if (m_producer != nullptr && m_producer != producer) {
            throw std::logic_error(
                "A RenderSurface may have only one active RenderView producer; "
                "present its color texture through a dynamic Image provider "
                "for additional consumers");
        }
        m_producer = producer;
    }

    void RenderSurface::detachProducer(const void *producer) noexcept {
        if (m_producer == producer) {
            m_producer = nullptr;
        }
    }

} // namespace Bess::UI
