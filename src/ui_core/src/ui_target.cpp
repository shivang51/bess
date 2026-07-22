#include "ui_target.h"
#include "bess_core/style/bess_theme.h"
#include "common/bess_assert.h"
#include "ui_painter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

namespace Bess::UI {
    namespace {
        [[nodiscard]] float finiteCoordinate(float value) noexcept {
            return std::isfinite(value) ? value : 0.f;
        }

        [[nodiscard]] float finiteExtent(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        [[nodiscard]] uint32_t attachmentExtent(float value) noexcept {
            constexpr double maximum =
                static_cast<double>(std::numeric_limits<uint32_t>::max());
            return static_cast<uint32_t>(
                std::min(static_cast<double>(finiteExtent(value)), maximum));
        }
    } // namespace

    UITarget::UITarget() : m_viewHost(m_widgetTree), m_popupHost(m_viewHost) {
    }

    UITarget::~UITarget() {
        try {
            destroy();
        } catch (...) {
            // Explicit destroy() remains the observable teardown path. A
            // destructor must never terminate because application-owned drag
            // or unmount callbacks failed.
        }
    }

    void
    UITarget::init(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
                   const UITargetDesc &desc) {
        if (renderer == nullptr) {
            throw std::invalid_argument(
                "UITarget requires an initialized renderer");
        }

        destroy();
        m_rect = {
            .pos = {finiteCoordinate(desc.rect.pos.x),
                    finiteCoordinate(desc.rect.pos.y)},
            .size = {finiteExtent(desc.rect.size.x),
                     finiteExtent(desc.rect.size.y)},
        };
        m_pickingStrategy = desc.pickingStrategy;
        m_contentScale =
            std::isfinite(desc.contentScale) && desc.contentScale > 0.f
                ? desc.contentScale
                : 1.f;
        m_widgetTree.setTheme(desc.theme != nullptr
                                  ? UITheme::fromBessTheme(*desc.theme)
                                  : UITheme::dark());
        m_widgetTree.setPlatformServices(desc.platformServices);
        m_widgetTree.setActionRegistry(desc.actionRegistry);
        m_widgetTree.setDragDropService(desc.dragDropService);
        auto renderTarget = renderer->createTarget({
            .extent =
                {
                    .width = attachmentExtent(m_rect.size.x),
                    .height = attachmentExtent(m_rect.size.y),
                },
            .targetFormat = desc.targetFormat,
            .pickingFormat = desc.pickingFormat,
            .surface = desc.surface,
        });
        if (renderTarget == nullptr) {
            throw std::runtime_error(
                "Renderer failed to create a UITarget render target");
        }
        m_renderer = renderer;
        m_renderTarget = std::move(renderTarget);

        m_widgetTree.setViewportSize(m_rect.size);
    }

    void UITarget::setTheme(const Core::Style::BessTheme &theme) {
        m_widgetTree.setTheme(UITheme::fromBessTheme(theme));
    }

    void UITarget::destroy() {
        std::exception_ptr failure;
        const auto rememberFailure = [&failure](auto &&operation) {
            try {
                std::invoke(std::forward<decltype(operation)>(operation));
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
        };

        m_popupHost.clear();
        m_viewHost.clear();
        // Destroy offscreen producers and all other widget-owned resources
        // while the shared renderer/device is still alive.
        rememberFailure([this] { m_widgetTree.clear(); });
        if (m_renderTarget != nullptr) {
            rememberFailure([this] { m_renderTarget->destroy(); });
            m_renderTarget.reset();
        }
        m_renderer.reset();
        m_pendingEvents.clear();
        m_frameEvents.clear();
        m_inputCtx = {};
        m_hasMousePos = false;
        m_pickingStrategy = UITargetPickingStrategy::cpuHitTest;
        m_lastDeltaTime = TimeMs{0};
        m_contentScale = 1.f;
        m_widgetTree.setPlatformServices({});
        m_widgetTree.setActionRegistry({});
        m_widgetTree.setDragDropService({});
        m_widgetTree.setViewportSize({0.f, 0.f});
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    std::shared_ptr<Core::Renderer::ITexture>
    UITarget::getColorTexture() const {
        return m_renderTarget != nullptr ? m_renderTarget->getColorTexture()
                                         : nullptr;
    }

    std::shared_ptr<Core::Renderer::ITexture>
    UITarget::getPickingTexture() const {
        return m_renderTarget != nullptr ? m_renderTarget->getPickingTexture()
                                         : nullptr;
    }

    void UITarget::enqueueEvent(UIEvent event) {
        m_pendingEvents.emplace_back(std::move(event));
    }

    void UITarget::enqueueEvent(Input::Event event) {
        const auto modifiers = event.modifiers;
        std::visit(
            [this, modifiers](auto &&inputEvent) {
                enqueueEvent(UIEvent{std::move(inputEvent), modifiers});
            },
            std::move(event.data));
    }

    std::span<const UIEvent> UITarget::getFrameEvents() const noexcept {
        return m_frameEvents;
    }

    const UITargetInpCtx &UITarget::getInputContext() const noexcept {
        return m_inputCtx;
    }

    CursorIcon UITarget::getCursorShape() const noexcept {
        return m_widgetTree.getCursorShape();
    }

    WidgetTree &UITarget::getWidgetTree() noexcept {
        return m_widgetTree;
    }

    const WidgetTree &UITarget::getWidgetTree() const noexcept {
        return m_widgetTree;
    }

    UIViewHost &UITarget::getViewHost() noexcept {
        return m_viewHost;
    }

    const UIViewHost &UITarget::getViewHost() const noexcept {
        return m_viewHost;
    }

    PopupHost &UITarget::getPopupHost() noexcept {
        return m_popupHost;
    }

    const PopupHost &UITarget::getPopupHost() const noexcept {
        return m_popupHost;
    }

    UIViewRef<UIView> UITarget::setContent(std::unique_ptr<UIView> view) {
        return m_viewHost.setContent(std::move(view));
    }

    UIViewRef<UIView> UITarget::mountOverlay(std::unique_ptr<UIView> view) {
        return m_viewHost.mountOverlay(std::move(view));
    }

    UIViewRef<UIView> UITarget::mountModal(std::unique_ptr<UIView> view) {
        return m_viewHost.mountModal(std::move(view));
    }

    bool UITarget::unmountView(ViewId id) {
        return m_viewHost.unmount(id);
    }

    void UITarget::resize(const glm::vec2 &size) {
        const glm::vec2 safeSize{
            std::isfinite(size.x) ? std::max(0.f, size.x) : 0.f,
            std::isfinite(size.y) ? std::max(0.f, size.y) : 0.f,
        };
        const bool changed = m_rect.size != safeSize;
        m_rect.size = safeSize;
        if (changed && m_renderTarget != nullptr && safeSize.x > 0.f &&
            safeSize.y > 0.f) {
            m_renderTarget->resize({
                .width = attachmentExtent(safeSize.x),
                .height = attachmentExtent(safeSize.y),
            });
        }

        m_widgetTree.setViewportSize(safeSize);
    }

    void UITarget::draw() {
        const auto &renderer = m_renderer;
        if (renderer == nullptr || m_renderTarget == nullptr) {
            throw std::logic_error("Cannot draw an uninitialized UITarget");
        }

        m_widgetTree.prepareRender(renderer, m_lastDeltaTime, m_contentScale);

        beginFrame(m_widgetTree.theme().canvas);

        const auto activeColor = m_renderTarget->getColorTexture();
        const auto activePicking = m_renderTarget->getPickingTexture();
        RendererUIPainter painter{
            *renderer,
            m_rect.size,
            activeColor != nullptr ? activeColor->getHandle() : 0,
            activePicking != nullptr ? activePicking->getHandle() : 0,
        };
        std::exception_ptr failure;
        try {
            m_widgetTree.paint(painter);
            m_viewHost.flushPendingUnmounts();
        } catch (...) {
            failure = std::current_exception();
        }
        try {
            m_renderTarget->endFrame();
        } catch (...) {
            if (failure == nullptr) {
                failure = std::current_exception();
            }
        }
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    void UITarget::update(TimeMs dt) {
        m_lastDeltaTime = dt;
        m_popupHost.update();
        processInputEvents();
        const auto ensureLayout = [this] {
            if (hasInvalidation(m_widgetTree.pendingInvalidation(),
                                WidgetInvalidation::layout)) {
                m_widgetTree.performLayout();
            }
        };

        // Hit testing uses a complete geometry snapshot, including on the
        // first frame after widgets are mounted or the target is resized. A
        // stable retained tree does not need to pay for another full Yoga and
        // arrange pass every frame.
        ensureLayout();
        for (const auto &event : m_frameEvents) {
            static_cast<void>(m_widgetTree.dispatchEvent(event));
            m_viewHost.flushPendingUnmounts();
            ensureLayout();
        }
        m_widgetTree.update(dt);
        m_viewHost.flushPendingUnmounts();
        ensureLayout();

        if (!m_hasMousePos || m_inputCtx.mousePos.x < 0.f ||
            m_inputCtx.mousePos.y < 0.f ||
            m_inputCtx.mousePos.x >= m_rect.size.x ||
            m_inputCtx.mousePos.y >= m_rect.size.y) {
            m_inputCtx.pickingId = PickingId::invalid();
            return;
        }

        if (m_pickingStrategy == UITargetPickingStrategy::cpuHitTest) {
            const glm::vec2 uiPosition =
                m_inputCtx.mousePos - m_rect.size * 0.5f;
            m_inputCtx.pickingId =
                m_widgetTree.getPickingId(m_widgetTree.hitTest(uiPosition));
            return;
        }

        m_inputCtx.pickingId =
            m_renderTarget != nullptr
                ? m_renderTarget->readPickingId(
                      static_cast<uint32_t>(m_inputCtx.mousePos.x),
                      static_cast<uint32_t>(m_inputCtx.mousePos.y))
                : PickingId::invalid();
    }

    void UITarget::processInputEvents() {
        m_frameEvents.clear();
        m_frameEvents.swap(m_pendingEvents);

        m_inputCtx.mouseDelta = {0.f, 0.f};
        m_inputCtx.mouseWheelDelta = {0.f, 0.f};

        for (auto &event : m_frameEvents) {
            if (const auto *resizeEvent = event.getIf<UITargetResizeEvent>()) {
                // A resize event is a complete target contract, not merely a
                // widget notification. This keeps offscreen/custom target
                // integrations correct even when they only enqueue events.
                resize({static_cast<float>(resizeEvent->width),
                        static_cast<float>(resizeEvent->height)});
            }
            if (!event.is<UITargetResizeEvent>()) {
                m_inputCtx.modifiers = event.modifiers;
            }

            if (auto *mouseMove = event.getIf<Input::MouseMoveEvent>()) {
                mouseMove->delta = m_hasMousePos
                                       ? mouseMove->pos - m_inputCtx.mousePos
                                       : glm::vec2{0.f, 0.f};
                m_inputCtx.mouseDelta += mouseMove->delta;
                m_inputCtx.mousePos = mouseMove->pos;
                m_hasMousePos = true;
                continue;
            }

            if (const auto *mouseWheel =
                    event.getIf<Input::MouseWheelEvent>()) {
                m_inputCtx.mousePos = mouseWheel->pos;
                m_inputCtx.mouseWheelDelta += mouseWheel->offset;
                m_hasMousePos = true;
                continue;
            }

            if (const auto *mouseButton =
                    event.getIf<Input::MouseButtonEvent>()) {
                m_inputCtx.mousePos = mouseButton->pos;
                m_hasMousePos = true;
            }
        }
    }

    void UITarget::beginFrame(const Core::Renderer::Color &background) {
        BESS_ASSERT(m_renderer != nullptr, "Renderer is not initialized");
        BESS_ASSERT(m_renderTarget != nullptr,
                    "Render target is not initialized");
        m_renderTarget->beginFrame({
            .clearColor = background,
            .shouldClear = true,
        });
    }

} // namespace Bess::UI
