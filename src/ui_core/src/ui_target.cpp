#include "ui_target.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/style/color_scheme.h"
#include "common/bess_assert.h"

#include <cstdint>
#include <format>

namespace Bess::UI {
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
    }

    void UITarget::destroy() {
        if (m_renderTarget != nullptr) {
            m_renderTarget->destroy();
            m_renderTarget.reset();
        }
        m_renderer.reset();
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

    void UITarget::setMousePos(const glm::vec2 &pos) {
        m_inputCtx.mousePos = pos;
    }

    void UITarget::resize(const glm::vec2 &size) {
        m_rect.size = size;
        if (m_renderTarget != nullptr && size.x > 0.f && size.y > 0.f) {
            m_renderTarget->resize({
                .width = static_cast<uint32_t>(size.x),
                .height = static_cast<uint32_t>(size.y),
            });
        }
    }

    void UITarget::draw() {
        const auto &renderer = m_renderer;

        beginFrame(Core::Renderer::Colors::darkGray);

        renderer->drawQuad({
            .position = {0, 0},
            .size = {100, 100},
            .color = Core::Renderer::Colors::teal,
            .id = {1},
            .transformMode = Core::Renderer::RenderTransformMode::Screen,
        });

        renderer->drawFont(
            std::format("({}, {}) | {}",
                        m_inputCtx.mousePos.x,
                        m_inputCtx.mousePos.y,
                        static_cast<uint64_t>(m_inputCtx.pickingId)),
            {
                .position = {0, 130},
                .fontSize = 20,
                .color = Core::Renderer::Colors::white,
                .transformMode = Core::Renderer::RenderTransformMode::Screen,
            });

        m_renderTarget->endFrame();
    }

    void UITarget::update(TimeMs dt) {
        static_cast<void>(dt);
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
