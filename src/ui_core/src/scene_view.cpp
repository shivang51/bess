#include "controls/scene_view.h"

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
        constexpr float kSceneViewContentZ = 0.001f;

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

        [[nodiscard]] Core::Renderer::TextureHandle
        textureHandle(const std::shared_ptr<Core::Renderer::ITexture> &texture) {
            return texture != nullptr ? texture->getHandle() : 0;
        }

        [[nodiscard]] BoxPaint boxPaint(WidgetBounds bounds,
                                        const UIBoxStyle &style,
                                        PickingId pickingId) {
            return {
                .bounds = bounds,
                .color = style.background,
                .borderColor = style.border,
                .cornerRadius = style.cornerRadius,
                .borderThickness = style.borderThickness,
                .shadow = style.shadow,
                .zIndex = kSceneViewContentZ,
                .pickingId = pickingId,
            };
        }
    } // namespace

    ISceneViewDelegate::~ISceneViewDelegate() = default;

    void ISceneViewDelegate::onAttach(SceneView &) {
    }

    void ISceneViewDelegate::onDetach(SceneView &) noexcept {
    }

    void ISceneViewDelegate::update(SceneViewUpdateContext &) {
    }

    UIEventReply ISceneViewDelegate::onEvent(SceneView &,
                                             WidgetEventContext &,
                                             const UIEvent &) {
        return {};
    }

    CursorIcon
    ISceneViewDelegate::cursor(const SceneView &,
                               const WidgetCursorContext &) const noexcept {
        return CursorIcon::inherit;
    }

    SceneView::SceneView(std::shared_ptr<ISceneViewDelegate> delegate,
                         SceneViewOptions options)
        : m_options(std::move(options)),
          m_delegate(std::move(delegate)) {
        m_options.target.initialExtent.width =
            std::max(1U, m_options.target.initialExtent.width);
        m_options.target.initialExtent.height =
            std::max(1U, m_options.target.initialExtent.height);
        if (!std::isfinite(m_options.renderScale) ||
            m_options.renderScale <= 0.f) {
            m_options.renderScale = 1.f;
        }
    }

    std::string_view SceneView::typeName() const noexcept {
        return "SceneView";
    }

    WidgetTraits SceneView::traits() const noexcept {
        return {
            .focusable = m_options.focusable,
            .hitTestVisible = m_options.hitTestVisible,
            .clipChildren = false,
            .preparesRender = true,
        };
    }

    void SceneView::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        context.layout.setWidthPercent(1.f);
        context.layout.setHeightPercent(1.f);
        synchronizeDelegate();
        requestRender();
    }

    void SceneView::onUnmount(WidgetTree &, WidgetId) {
        m_state = nullptr;
        m_id = {};
        destroyTarget();
        m_renderer.reset();
        const auto attached = std::exchange(m_attachedDelegate, {});
        if (attached != nullptr) {
            attached->onDetach(*this);
        }
    }

    void SceneView::update(WidgetUpdateContext &context) {
        const auto delegate = m_delegate;
        if (delegate == nullptr) {
            return;
        }
        const auto bounds = context.state.getBounds(context.id);
        auto layoutExtent = desiredExtent(bounds, 1.f);
        if (layoutExtent.width == 0 || layoutExtent.height == 0) {
            layoutExtent = extent();
        }
        SceneViewUpdateContext update{
            .view = *this,
            .bounds = bounds,
            .deltaTime = context.deltaTime,
            .extent = layoutExtent,
            .treeViewportSize = context.state.getViewportSize(),
            .hasTargets = m_target != nullptr,
        };
        delegate->update(update);
    }

    void SceneView::prepareRender(WidgetRenderPrepareContext &context) {
        if (context.renderer == nullptr) {
            return;
        }
        if (!context.effectivelyVisible &&
            m_options.policy != RenderPolicy::continuous) {
            return;
        }

        ensureTarget(context.renderer);

        auto desired = desiredExtent(context.bounds, context.contentScale);
        if (desired.width == 0 || desired.height == 0) {
            if (m_options.policy != RenderPolicy::continuous) {
                return;
            }
            desired = extent();
            if (desired.width == 0 || desired.height == 0) {
                return;
            }
        }

        const bool resized = resizeTarget(desired);
        if (!shouldRender(context.effectivelyVisible, resized)) {
            return;
        }

        auto color = colorTexture();
        auto picking = pickingTexture();
        const auto colorHandle = textureHandle(color);
        const auto pickingHandle = textureHandle(picking);
        if (colorHandle == 0 || pickingHandle == 0) {
            return;
        }

        SceneViewFrameContext frameContext{
            .view = *this,
            .renderer = *context.renderer,
            .bounds = context.bounds,
            .extent = desired,
            .colorTarget = colorHandle,
            .pickingTarget = pickingHandle,
            .colorTexture = std::move(color),
            .pickingTexture = std::move(picking),
            .deltaTime = context.deltaTime,
            .treeViewportSize = context.state.getViewportSize(),
            .effectivelyVisible = context.effectivelyVisible,
            .resized = resized,
        };

        m_renderRequested = false;
        try {
            if (m_delegate != nullptr) {
                m_delegate->render(frameContext);
            }
        } catch (...) {
            m_renderRequested = true;
            throw;
        }
    }

    void SceneView::paint(WidgetPaintContext &context) const {
        if (context.bounds.empty()) {
            return;
        }

        auto texture = colorTexture();
        if (texture == nullptr || texture->getHandle() == 0) {
            if (m_options.placeholder.has_value()) {
                context.painter.drawBox(boxPaint(
                    context.bounds, *m_options.placeholder, context.pickingId));
            }
            return;
        }

        context.painter.drawImage({
            .bounds = context.bounds,
            .texture = std::move(texture),
            .uvRect = {0.f, 0.f, 1.f, 1.f},
            .tint = m_options.tint,
            .cornerRadius = m_options.cornerRadius,
            .zIndex = kSceneViewContentZ,
            .pickingId = context.pickingId,
        });
    }

    UIEventReply SceneView::onEvent(WidgetEventContext &context,
                                    const UIEvent &event) {
        return m_delegate != nullptr
                   ? m_delegate->onEvent(*this, context, event)
                   : UIEventReply{};
    }

    CursorIcon
    SceneView::cursor(const WidgetCursorContext &context) const noexcept {
        return m_delegate != nullptr ? m_delegate->cursor(*this, context)
                                     : CursorIcon::inherit;
    }

    void SceneView::requestRender() noexcept {
        m_renderRequested = true;
        if (m_state != nullptr && m_id) {
            m_state->invalidate(m_id, WidgetInvalidation::paint);
        }
    }

    bool SceneView::renderRequested() const noexcept {
        return m_renderRequested;
    }

    RenderPolicy SceneView::renderPolicy() const noexcept {
        return m_options.policy;
    }

    void SceneView::setRenderPolicy(RenderPolicy policy) noexcept {
        m_options.policy = policy;
        requestRender();
    }

    Core::Renderer::Renderer2DExtent SceneView::extent() const noexcept {
        return m_target != nullptr ? m_target->getExtent()
                                   : m_options.target.initialExtent;
    }

    std::shared_ptr<Core::Renderer::ITexture> SceneView::colorTexture() const {
        return m_target != nullptr ? m_target->getColorTexture() : nullptr;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    SceneView::pickingTexture() const {
        return m_target != nullptr ? m_target->getPickingTexture() : nullptr;
    }

    Core::Renderer::TextureHandle SceneView::colorHandle() const {
        return textureHandle(colorTexture());
    }

    Core::Renderer::TextureHandle SceneView::pickingHandle() const {
        return textureHandle(pickingTexture());
    }

    bool SceneView::requestPickingId(uint32_t x, uint32_t y) {
        return requestPickingIds(x, y, 1, 1);
    }

    bool SceneView::requestPickingIds(uint32_t x,
                                      uint32_t y,
                                      uint32_t width,
                                      uint32_t height) {
        if (m_renderer == nullptr || m_target == nullptr || width == 0 ||
            height == 0) {
            return false;
        }
        const auto handle = pickingHandle();
        if (handle == 0) {
            return false;
        }
        const auto size = m_target->getExtent();
        if (x >= size.width || y >= size.height) {
            return false;
        }
        m_renderer->requestPickingIds({
            .texture = handle,
            .x = x,
            .y = y,
            .width = width,
            .height = height,
        });
        return true;
    }

    bool SceneView::tryGetPickingIds(
        Core::Renderer::PickingReadbackResult &result) const {
        if (m_renderer == nullptr) {
            return false;
        }
        return m_renderer->tryGetPickingIds(result);
    }

    PickingId SceneView::readPickingId(uint32_t x, uint32_t y) const {
        if (m_target == nullptr) {
            return PickingId::invalid();
        }
        const auto size = m_target->getExtent();
        return x < size.width && y < size.height ? m_target->readPickingId(x, y)
                                                 : PickingId::invalid();
    }

    const std::shared_ptr<ISceneViewDelegate> &
    SceneView::delegate() const noexcept {
        return m_delegate;
    }

    void
    SceneView::setDelegate(std::shared_ptr<ISceneViewDelegate> delegate) {
        if (m_delegate == delegate) {
            synchronizeDelegate();
            return;
        }
        m_delegate = std::move(delegate);
        synchronizeDelegate();
        requestRender();
    }

    void SceneView::destroyTarget() noexcept {
        if (m_target == nullptr) {
            return;
        }
        try {
            m_target->destroy();
        } catch (...) {
        }
        m_target.reset();
    }

    void SceneView::ensureTarget(
        const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
        if (renderer == nullptr) {
            throw std::invalid_argument(
                "SceneView requires an initialized renderer");
        }
        if (m_target != nullptr) {
            if (m_renderer.get() != renderer.get()) {
                throw std::logic_error(
                    "A live SceneView cannot switch renderers");
            }
            return;
        }

        auto target = renderer->createTarget({
            .extent = m_options.target.initialExtent,
            .targetFormat = m_options.target.colorFormat.value_or(
                renderer->getTargetFormatType()),
            .pickingFormat = m_options.target.pickingFormat.value_or(
                renderer->getPickingFormatType()),
            .surface = {},
        });
        if (target == nullptr) {
            throw std::runtime_error(
                "Renderer failed to create a SceneView target");
        }
        m_renderer = renderer;
        m_target = std::move(target);
    }

    bool SceneView::resizeTarget(Core::Renderer::Renderer2DExtent extent) {
        if (m_target == nullptr) {
            throw std::logic_error("Cannot resize an uninitialized SceneView");
        }
        if (extent.width == 0 || extent.height == 0) {
            throw std::invalid_argument("SceneView extent must be non-zero");
        }
        const auto current = m_target->getExtent();
        if (current.width == extent.width && current.height == extent.height) {
            return false;
        }
        m_target->resize(extent);
        return true;
    }

    void SceneView::synchronizeDelegate() {
        if (m_state == nullptr || m_synchronizingDelegate) {
            return;
        }

        m_synchronizingDelegate = true;
        std::exception_ptr attachmentFailure;
        try {
            size_t remainingTransitions = 64;
            while (m_state != nullptr && m_attachedDelegate != m_delegate) {
                if (remainingTransitions-- == 0U) {
                    throw std::logic_error(
                        "SceneView delegate lifecycle did not converge");
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
                        }
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
    SceneView::desiredExtent(WidgetBounds bounds,
                             float contentScale) const noexcept {
        const float scale = contentScale * m_options.renderScale;
        return {
            .width = physicalExtent(bounds.size.x, scale),
            .height = physicalExtent(bounds.size.y, scale),
        };
    }

    bool SceneView::shouldRender(bool effectivelyVisible,
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
