#include "ui_target.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/style/color_scheme.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "dock.h"

#include <cstdint>
#include <format>
#include <utility>

namespace Bess::UI {

    namespace {
        void createTestDock(DockManager &dockManager) {
            // auto dock = dockManager.createNode<DockLeaf>();
            // dockManager.setRootNode(dock);

            std::vector<std::shared_ptr<Bess::UI::DockLeaf>> leaves;
            for (int i = 0; i < 4; ++i) {
                auto leaf = dockManager.createNode<Bess::UI::DockLeaf>();
                leaves.push_back(leaf);
                if (i == 0) {
                    leaf->setSize(glm::vec2(800.0f, 600.0f));
                    dockManager.dockNode(leaf->getId(),
                                         dockManager.getRootNode(),
                                         Bess::UI::DockZone::main);
                } else {
                    dockManager.dockNode(leaf->getId(),
                                         dockManager.getRootNode(),
                                         Bess::UI::DockZone::right);
                }
            }

            auto leaf = dockManager.createNode<Bess::UI::DockLeaf>();
            dockManager.dockNode(leaf->getId(),
                                 dockManager.getRootNode(),
                                 Bess::UI::DockZone::bottom);
        }

        uint32_t dockRuntimeId = 0;

        UUID hitDock = UUID::null;

        void drawDockNode(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            DockManager &dockManager,
            const UUID &nodeId) {
            auto node = dockManager.getNode(nodeId);
            if (!node) {
                return;
            }

            const bool isHit = nodeId == hitDock;

            if (node->isLeaf()) {
                auto topLeft = node->getPos();
                auto size = node->getSize();
                auto center = topLeft + size * 0.5f;
                renderer->drawQuad({
                    .position = center,
                    .size = size,
                    .color = isHit ? Core::Renderer::Colors::limeGreen
                                   : Core::Renderer::Colors::pastelRed,
                    .id = {.runtimeId = dockRuntimeId++},
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });
            } else if (node->isTab()) {
                auto topLeft = node->getPos();
                auto size = node->getSize();
                auto center = topLeft + size * 0.5f;
                renderer->drawQuad({
                    .position = center,
                    .size = size,
                    .color = isHit ? Core::Renderer::Colors::limeGreen
                                   : Core::Renderer::Colors::blue,
                    .id = {.runtimeId = dockRuntimeId++},
                    .transformMode =
                        Core::Renderer::RenderTransformMode::Screen,
                });

                auto tabNode = std::dynamic_pointer_cast<DockTab>(node);
                renderer->drawFont(
                    std::format("Tab Node with {} children",
                                tabNode->getDockedNodes().size()),
                    {
                        .position = center,
                        .fontSize = 14,
                        .color = Core::Renderer::Colors::white,
                        .transformMode =
                            Core::Renderer::RenderTransformMode::Screen,
                    });

                for (const auto &childId : tabNode->getDockedNodes()) {
                    drawDockNode(renderer, dockManager, childId);
                }
            } else if (node->isSplitter()) {
                auto topLeft = node->getPos();
                auto size = node->getSize();

                auto splitNode = std::dynamic_pointer_cast<DockSplitter>(node);
                const auto &splitNodes = splitNode->getSplitNodes();
                drawDockNode(renderer, dockManager, splitNodes.first);
                drawDockNode(renderer, dockManager, splitNodes.second);

                if (splitNode->getSplitDir() == SplitDirection::horizontal) {
                    renderer->drawLine({
                        .p0 = {topLeft.x,
                               topLeft.y +
                                   (size.y * splitNode->getSplitRatio())},
                        .p1 = {topLeft.x + size.x,
                               topLeft.y +
                                   (size.y * splitNode->getSplitRatio())},
                        .thickness = 2.0f,
                        .zIndex = 2,
                        .color = Core::Renderer::Colors::white,
                        .id = {.runtimeId = dockRuntimeId++},
                        .transformMode =
                            Core::Renderer::RenderTransformMode::Screen,
                    });
                } else {
                    renderer->drawLine({
                        .p0 = {topLeft.x +
                                   (size.x * splitNode->getSplitRatio()),
                               topLeft.y},
                        .p1 = {topLeft.x +
                                   (size.x * splitNode->getSplitRatio()),
                               topLeft.y + size.y},
                        .thickness = 2.0f,
                        .zIndex = 2,
                        .color = Core::Renderer::Colors::white,
                        .id = {.runtimeId = dockRuntimeId++},
                        .transformMode =
                            Core::Renderer::RenderTransformMode::Screen,
                    });
                }
            }
        }

        void
        drawDock(DockManager &dockManager,
                 const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer) {
            dockRuntimeId = 0;
            auto rootNode = dockManager.getNode(dockManager.getRootNode());
            if (!rootNode) {
                return;
            }

            drawDockNode(renderer, dockManager, rootNode->getId());
        }

    } // namespace

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

        m_dockManager.init();
        createTestDock(m_dockManager);
        m_dockManager.setSize(m_rect.size);
        m_dockManager.setPos(m_rect.size * -0.5f);
    }

    void UITarget::destroy() {
        if (m_renderTarget != nullptr) {
            m_renderTarget->destroy();
            m_renderTarget.reset();
        }
        m_renderer.reset();
        m_pendingEvents.clear();
        m_frameEvents.clear();
        m_inputCtx = {};
        m_hasMousePos = false;
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

    void UITarget::resize(const glm::vec2 &size) {
        m_rect.size = size;
        if (m_renderTarget != nullptr && size.x > 0.f && size.y > 0.f) {
            m_renderTarget->resize({
                .width = static_cast<uint32_t>(size.x),
                .height = static_cast<uint32_t>(size.y),
            });
        }

        m_dockManager.setSize(size);
        m_dockManager.setPos(size * -0.5f);
    }

    void UITarget::draw() {
        const auto &renderer = m_renderer;

        beginFrame(Core::Renderer::Colors::darkGray);

        drawDock(m_dockManager, renderer);

        // renderer->drawQuad({
        //     .position = {0, 0},
        //     .size = {100, 100},
        //     .color = Core::Renderer::Colors::teal,
        //     .id = {1},
        //     .transformMode = Core::Renderer::RenderTransformMode::Screen,
        // });
        //
        // renderer->drawFont(
        //     std::format("({}, {}) | {}",
        //                 m_inputCtx.mousePos.x,
        //                 m_inputCtx.mousePos.y,
        //                 static_cast<uint64_t>(m_inputCtx.pickingId)),
        //     {
        //         .position = {0, 130},
        //         .fontSize = 20,
        //         .color = Core::Renderer::Colors::white,
        //         .transformMode = Core::Renderer::RenderTransformMode::Screen,
        //     });
        //
        m_renderTarget->endFrame();
    }

    void UITarget::update(TimeMs dt) {
        static_cast<void>(dt);

        processInputEvents();
        m_dockManager.layout();

        if (m_renderTarget == nullptr || m_inputCtx.mousePos.x < 0.f ||
            m_inputCtx.mousePos.y < 0.f ||
            m_inputCtx.mousePos.x >= m_rect.size.x ||
            m_inputCtx.mousePos.y >= m_rect.size.y) {
            m_inputCtx.pickingId = PickingId::invalid();
            return;
        }

        const auto pos = m_inputCtx.mousePos - (m_rect.size * 0.5f);
        hitDock = m_dockManager.getHitRect(pos);
        BESS_TRACE("Hit dock: {} | mousePos = {}", hitDock, pos);

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
