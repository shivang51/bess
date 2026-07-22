#include "controls/render_view.h"

#include "bess_core/renderer/texture.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kRenderViewContentZ = 0.001f;

        [[nodiscard]] uint32_t physicalExtent(float logical,
                                              float scale) noexcept {
            if (!std::isfinite(logical) || !std::isfinite(scale) ||
                logical <= 0.f || scale <= 0.f) {
                return 0;
            }
            const double scaled = static_cast<double>(logical) * scale;
            const double maximum =
                static_cast<double>(std::numeric_limits<uint32_t>::max());
            return static_cast<uint32_t>(
                std::clamp(std::round(scaled), 1.0, maximum));
        }
    } // namespace

    IRenderViewDelegate::~IRenderViewDelegate() = default;

    void IRenderViewDelegate::onAttach(RenderView &) {
    }

    void IRenderViewDelegate::onDetach(RenderView &) noexcept {
    }

    void IRenderViewDelegate::update(RenderViewUpdateContext &) {
    }

    void IRenderViewDelegate::configureFrame(
        RenderViewFrameContext &, Core::Renderer::RenderTarget2DFrameInfo &) {
    }

    UIEventReply IRenderViewDelegate::onEvent(RenderView &,
                                              WidgetEventContext &,
                                              const UIEvent &) {
        return {};
    }

    CursorIcon
    IRenderViewDelegate::cursor(const RenderView &,
                                const WidgetCursorContext &) const noexcept {
        return CursorIcon::inherit;
    }

    RenderView::RenderView(std::shared_ptr<IRenderViewDelegate> delegate,
                           RenderViewOptions options,
                           std::shared_ptr<RenderSurface> surface)
        : m_options(std::move(options)),
          m_surface(surface != nullptr
                        ? std::move(surface)
                        : std::make_shared<RenderSurface>(m_options.surface)),
          m_delegate(std::move(delegate)) {
        if (!std::isfinite(m_options.renderScale) ||
            m_options.renderScale <= 0.f) {
            m_options.renderScale = 1.f;
        }
    }

    std::string_view RenderView::typeName() const noexcept {
        return "RenderView";
    }

    WidgetTraits RenderView::traits() const noexcept {
        return {
            .focusable = m_options.focusable,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = false,
            .preparesRender = true,
        };
    }

    void RenderView::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        context.layout.setWidthPercent(1.f);
        context.layout.setHeightPercent(1.f);
        if (m_surface != nullptr) {
            m_surface->attachProducer(this);
        }
        synchronizeDelegate();
        requestRender();
    }

    void RenderView::onUnmount(WidgetTree &, WidgetId) {
        // Make re-entrant setters observe an unmounted view before calling
        // application code. The configured delegate is deliberately retained
        // so a later mount attaches the latest requested delegate.
        m_state = nullptr;
        m_id = {};
        if (m_surface != nullptr) {
            m_surface->detachProducer(this);
        }
        const auto attached = std::exchange(m_attachedDelegate, {});
        if (attached != nullptr) {
            attached->onDetach(*this);
        }
    }

    void RenderView::update(WidgetUpdateContext &context) {
        const auto surface = m_surface;
        const auto delegate = m_delegate;
        if (delegate != nullptr && surface != nullptr) {
            RenderViewUpdateContext update{
                .view = *this,
                .surface = *surface,
                .bounds = context.state.getBounds(context.id),
                .deltaTime = context.deltaTime,
            };
            delegate->update(update);
        }
    }

    void RenderView::prepareRender(WidgetRenderPrepareContext &context) {
        const auto surface = m_surface;
        if (surface == nullptr || context.renderer == nullptr) {
            return;
        }
        if (!context.effectivelyVisible &&
            m_options.policy != RenderPolicy::continuous) {
            return;
        }
        surface->initialize(context.renderer);

        auto desired = desiredExtent(context.bounds, context.contentScale);
        if (desired.width == 0 || desired.height == 0) {
            if (m_options.policy != RenderPolicy::continuous) {
                return;
            }
            desired = surface->extent();
            if (desired.width == 0 || desired.height == 0) {
                return;
            }
        }
        const bool resized = surface->resize(desired);
        if (!shouldRender(context.effectivelyVisible, resized)) {
            return;
        }

        auto renderer = surface->renderer();
        if (renderer == nullptr) {
            return;
        }
        RenderViewFrameContext frameContext{
            .view = *this,
            .surface = *surface,
            .renderer = *renderer,
            .bounds = context.bounds,
            .extent = desired,
            .deltaTime = context.deltaTime,
            .effectivelyVisible = context.effectivelyVisible,
        };
        auto frameInfo = m_options.frame;
        const auto delegate = m_delegate;
        // Clear immediately before application callbacks. A request raised by
        // configureFrame() or render() then remains pending for the following
        // frame; a failure restores the request for retry.
        m_renderRequested = false;
        try {
            if (delegate != nullptr) {
                delegate->configureFrame(frameContext, frameInfo);
            }
            surface->render(
                frameInfo,
                [&frameContext, delegate](Core::Renderer::IRenderer2D &,
                                          Core::Renderer::IRenderTarget2D &) {
                    if (delegate != nullptr) {
                        delegate->render(frameContext);
                    }
                });
        } catch (...) {
            m_renderRequested = true;
            throw;
        }
    }

    void RenderView::paint(WidgetPaintContext &context) const {
        if (m_surface == nullptr) {
            return;
        }
        auto texture = m_surface->colorTexture();
        if (texture == nullptr || texture->getHandle() == 0 ||
            context.bounds.empty()) {
            return;
        }
        context.painter.drawImage({
            .bounds = context.bounds,
            .texture = std::move(texture),
            .uvRect = {0.f, 0.f, 1.f, 1.f},
            .tint = m_options.tint,
            .cornerRadius = m_options.cornerRadius,
            .zIndex = kRenderViewContentZ,
            .pickingId = context.pickingId,
        });
    }

    UIEventReply RenderView::onEvent(WidgetEventContext &context,
                                     const UIEvent &event) {
        return m_delegate != nullptr
                   ? m_delegate->onEvent(*this, context, event)
                   : UIEventReply{};
    }

    CursorIcon
    RenderView::cursor(const WidgetCursorContext &context) const noexcept {
        return m_delegate != nullptr ? m_delegate->cursor(*this, context)
                                     : CursorIcon::inherit;
    }

    void RenderView::requestRender() noexcept {
        m_renderRequested = true;
        if (m_state != nullptr && m_id) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    bool RenderView::renderRequested() const noexcept {
        return m_renderRequested;
    }

    RenderPolicy RenderView::renderPolicy() const noexcept {
        return m_options.policy;
    }

    void RenderView::setRenderPolicy(RenderPolicy policy) noexcept {
        m_options.policy = policy;
        requestRender();
    }

    const std::shared_ptr<RenderSurface> &RenderView::surface() const noexcept {
        return m_surface;
    }

    void RenderView::setSurface(std::shared_ptr<RenderSurface> surface) {
        if (surface == nullptr) {
            surface = std::make_shared<RenderSurface>(m_options.surface);
        }
        if (m_surface == surface) {
            return;
        }
        if (m_state != nullptr) {
            surface->attachProducer(this);
            if (m_surface != nullptr) {
                m_surface->detachProducer(this);
            }
        }
        m_surface = std::move(surface);
        requestRender();
    }

    const std::shared_ptr<IRenderViewDelegate> &
    RenderView::delegate() const noexcept {
        return m_delegate;
    }

    void
    RenderView::setDelegate(std::shared_ptr<IRenderViewDelegate> delegate) {
        if (m_delegate == delegate) {
            // A previous onAttach() may have requested this same delegate and
            // then failed before the lifecycle reconciled. Treat assigning the
            // desired value again as an explicit retry while mounted.
            synchronizeDelegate();
            return;
        }
        m_delegate = std::move(delegate);
        synchronizeDelegate();
        requestRender();
    }

    void RenderView::synchronizeDelegate() {
        if (m_state == nullptr || m_synchronizingDelegate) {
            return;
        }

        m_synchronizingDelegate = true;
        std::exception_ptr attachmentFailure;
        try {
            // Lifecycle hooks are allowed to call setDelegate(). Reconcile to
            // the last requested value, with a finite guard against a pair of
            // adversarial delegates which continually replace each other.
            size_t remainingTransitions = 64;
            while (m_state != nullptr && m_attachedDelegate != m_delegate) {
                if (remainingTransitions-- == 0U) {
                    throw std::logic_error(
                        "RenderView delegate lifecycle did not converge");
                }
                const auto attached = std::exchange(m_attachedDelegate, {});
                if (attached != nullptr) {
                    attached->onDetach(*this);
                }
                if (m_state == nullptr) {
                    break;
                }
                const auto requested = m_delegate;
                if (requested != nullptr) {
                    // Publish attachment before entering application code so
                    // a re-entrant replacement is reconciled by this loop.
                    m_attachedDelegate = requested;
                    try {
                        requested->onAttach(*this);
                    } catch (...) {
                        if (attachmentFailure == nullptr) {
                            attachmentFailure = std::current_exception();
                        }
                        if (m_attachedDelegate == requested) {
                            m_attachedDelegate.reset();
                        }
                        if (m_delegate == requested) {
                            m_delegate.reset();
                        }
                        try {
                            requested->onDetach(*this);
                        } catch (...) {
                            // onDetach is noexcept by contract; preserve the
                            // original attachment failure defensively.
                        }
                        // onAttach() may have selected another delegate before
                        // failing. Continue until configured and attached state
                        // agree, then report the first lifecycle failure.
                    }
                }
            }
        } catch (...) {
            m_synchronizingDelegate = false;
            throw;
        }
        m_synchronizingDelegate = false;
        if (attachmentFailure != nullptr) {
            std::rethrow_exception(attachmentFailure);
        }
    }

    Core::Renderer::Renderer2DExtent
    RenderView::desiredExtent(WidgetBounds bounds,
                              float contentScale) const noexcept {
        const float scale = contentScale * m_options.renderScale;
        return {
            .width = physicalExtent(bounds.size.x, scale),
            .height = physicalExtent(bounds.size.y, scale),
        };
    }

    bool RenderView::shouldRender(bool effectivelyVisible,
                                  bool resized) const noexcept {
        switch (m_options.policy) {
        case RenderPolicy::onDemand:
            return effectivelyVisible && (m_renderRequested || resized);
        case RenderPolicy::whileVisible:
            return effectivelyVisible;
        case RenderPolicy::continuous:
            return true;
        }
        return false;
    }

} // namespace Bess::UI
