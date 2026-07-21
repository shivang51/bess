#include "ui_target.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/style/color_scheme.h"
#include "common/bess_assert.h"
#include "ui_painter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Bess::UI {

    UITarget::UITarget() : m_viewHost(m_widgetTree) {
    }

    UITarget::~UITarget() {
        destroy();
    }

    void
    UITarget::init(const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
                   const UITargetDesc &desc) {
        BESS_ASSERT(renderer != nullptr,
                    "UITarget requires an initialized renderer");

        destroy();
        m_renderer = renderer;
        m_rect = desc.rect;
        m_renderTarget = renderer->createTarget({
            .extent =
                {
                    .width = static_cast<uint32_t>(m_rect.size.x),
                    .height = static_cast<uint32_t>(m_rect.size.y),
                },
            .targetFormat = desc.targetFormat,
            .pickingFormat = desc.pickingFormat,
            .surface = desc.surface,
        });
        BESS_ASSERT(m_renderTarget != nullptr,
                    "Renderer failed to create a UITarget render target");

        m_widgetTree.setViewportSize(m_rect.size);
    }

    void UITarget::destroy() {
        m_viewHost.clear();
        if (m_renderTarget != nullptr) {
            m_renderTarget->destroy();
            m_renderTarget.reset();
        }
        m_renderer.reset();
        m_pendingEvents.clear();
        m_frameEvents.clear();
        m_inputCtx = {};
        m_hasMousePos = false;
        m_widgetTree.clear();
        m_widgetTree.setViewportSize({0.f, 0.f});
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
                .width = static_cast<uint32_t>(safeSize.x),
                .height = static_cast<uint32_t>(safeSize.y),
            });
        }

        m_widgetTree.setViewportSize(safeSize);
    }

    void UITarget::draw() {
        const auto &renderer = m_renderer;

        beginFrame(Core::Renderer::Colors::darkGray);

        RendererUIPainter painter{*renderer, m_rect.size};
        m_widgetTree.paint(painter);
        m_viewHost.flushPendingUnmounts();
        m_renderTarget->endFrame();
    }

    void UITarget::update(TimeMs dt) {
        processInputEvents();
        // Hit testing always uses a complete geometry snapshot, including on
        // the first frame after widgets are mounted or the target is resized.
        m_widgetTree.performLayout();
        for (const auto &event : m_frameEvents) {
            static_cast<void>(m_widgetTree.dispatchEvent(event));
            m_viewHost.flushPendingUnmounts();
            if (hasInvalidation(m_widgetTree.pendingInvalidation(),
                                WidgetInvalidation::layout)) {
                m_widgetTree.performLayout();
            }
        }
        m_widgetTree.update(dt);
        m_viewHost.flushPendingUnmounts();
        m_widgetTree.performLayout();

        if (m_renderTarget == nullptr || m_inputCtx.mousePos.x < 0.f ||
            m_inputCtx.mousePos.y < 0.f ||
            m_inputCtx.mousePos.x >= m_rect.size.x ||
            m_inputCtx.mousePos.y >= m_rect.size.y) {
            m_inputCtx.pickingId = PickingId::invalid();
            return;
        }

        m_inputCtx.pickingId = m_renderTarget->readPickingId(
            static_cast<uint32_t>(m_inputCtx.mousePos.x),
            static_cast<uint32_t>(m_inputCtx.mousePos.y));
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

    void UITarget::beginFrame(const Core::Style::Color &background) {
        BESS_ASSERT(m_renderer != nullptr, "Renderer is not initialized");
        BESS_ASSERT(m_renderTarget != nullptr,
                    "Render target is not initialized");
        m_renderTarget->beginFrame({
            .clearColor = background,
            .shouldClear = true,
        });
    }

} // namespace Bess::UI
