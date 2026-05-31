#include "ui_sub_system.h"
#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "pages/main_page/main_page.h"
#include "settings/settings.h"
#include "sub_systems/renderer_context.h"
#include "ui.h"
#include "vulkan_core.h"

namespace Bess {
    void UISubSystem::onInit() {
        BESS_INFO("[UISubSystem] Initializing UI SubSystem");

        const auto &appCtx = GAppContext::getInstance();

        BESS_ASSERT(appCtx.hasSubSystem<RendererContext>(),
                    "VulkanCore is required for UISubSystem initialization");
        BESS_ASSERT(appCtx.hasSubSystem<Window>(),
                    "Window is required for UISubSystem initialization");
    }

    void UISubSystem::onPostInit() {
        const auto &appCtx = GAppContext::getInstance();
        const auto &window = appCtx.getSubSystem<Window>();
        UI::init(window->getGLFWHandle());

        m_mainPage = Pages::MainPage::getInstance(window);
    }

    void UISubSystem::onDestroy() {
        if (m_previewTex) {
            m_previewTex.reset();
        }
        m_mainPage.reset();
        // TEMP: Will remove getInstance fn
        Pages::MainPage::getInstance().reset();
        UI::shutdown();
        BESS_INFO("[UISubSystem] Destroyed UI SubSystem");
    }

    void UISubSystem::onPreDraw() { UI::begin(); }

    void UISubSystem::onPostDraw() { UI::end(); }

    void UISubSystem::onShutdown() {
        const auto &appCtx = GAppContext::getInstance();
        if (appCtx.hasSubSystem<VulkanCore>() && m_mainPage) {
            m_mainPage->destory();
        }
    }

    void UISubSystem::onDraw() {
        // m_mainPage->draw();

        const auto &appCtx = GAppContext::getInstance();
        const auto &settings = appCtx.getSubSystem<Config::Settings>();

        if (settings->getShowStatsWindow()) {
            UI::drawStats(m_currentFps);
        }

        auto renderer = appCtx.getSubSystem<RendererContext>()
                            ->getRenderer<Wgpu::WgpuRenderer2D>();
        if (m_previewTex == nullptr) {
            m_previewTex = std::make_shared<Wgpu::WgpuTexture>(*renderer.get());
            m_previewTex->setSize({800, 800});
            m_previewTex->init();
        }

        Bess::Core::Renderer::Renderer2DFrameInfo frameInfo;
        frameInfo.clearColor = Core::Renderer::Color{0.0f, 0.0f, 0.0f, 1.0f};
        frameInfo.shouldClear = true;
        frameInfo.targetTexture = m_previewTex->getHandle();
        frameInfo.extent = {800, 800};
        renderer->beginFrame(frameInfo);
        constexpr size_t quadsPerRow = 200;
        constexpr size_t quadCount = quadsPerRow * quadsPerRow;
        constexpr size_t quadsColumns = quadCount / quadsPerRow;
        constexpr size_t quadW = 800 / quadsPerRow;
        constexpr size_t quadH = 800 / quadsColumns;
        for (int i = 0; i < quadCount; i++) {
            renderer->drawQuad(
                {.position = {(quadW * (i % quadsPerRow)) + (quadW / 2),
                              (quadH * (i / quadsPerRow)) + (quadH / 2)},
                 .size = {quadW, quadH},
                 .color = {(float)i / quadCount, 1 - ((float)i / quadCount),
                           0.f, 1.0f}});
        }
        renderer->endFrame();

        ImGui::Begin("Debug Window");
        ImGui::Text("FPS: %d", m_currentFps);
        ImGui::Text("Quad Count: %d", renderer->getStats().quadCount);
        ImGui::Image(
            reinterpret_cast<ImTextureID>(
                renderer->getTextureView(m_previewTex->getHandle()).Get()),
            ImVec2(800, 800));
        ImGui::End();
    }

    void UISubSystem::onUpdate(TimeMs dt) {
        m_mainPage->update(dt);
        m_currentFps = static_cast<int>(std::round(1000.0 / dt.count()));
    }

} // namespace Bess
