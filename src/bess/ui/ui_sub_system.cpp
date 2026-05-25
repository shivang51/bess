#include "ui_sub_system.h"
#include "application_state.h"
#include "bess_core/g_app_context.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "imgui_impl_vulkan.h"
#include "pages/main_page/main_page.h"
#include "settings/settings.h"
#include "ui.h"
#include "vulkan_core.h"

namespace Bess {
    void UISubSystem::onInit() {
        BESS_INFO("[UISubSystem] Initializing UI SubSystem");

        const auto &appCtx = GAppContext::getInstance();

        BESS_ASSERT(appCtx.hasSubSystem<Bess::Vulkan::VulkanCore>(),
                    "VulkanCore is required for UISubSystem initialization");
        BESS_ASSERT(appCtx.hasSubSystem<Window>(),
                    "Window is required for UISubSystem initialization");

        m_vkCore = appCtx.getSubSystem<Bess::Vulkan::VulkanCore>();
    }

    void UISubSystem::onPostInit() {
        const auto &appCtx = GAppContext::getInstance();
        const auto &window = appCtx.getSubSystem<Window>();
        UI::init(window->getGLFWHandle());

        m_mainPage = Pages::MainPage::getInstance(window);
    }

    void UISubSystem::onDestroy() {
        BESS_INFO("[UISubSystem] Destroyed UI SubSystem");
    }

    void UISubSystem::onShutdown() {
        Pages::MainPage::getInstance()->destory();
        Pages::MainPage::getInstance().reset();
        UI::shutdown();
    }

    void UISubSystem::onDraw() {
        UI::begin();

        m_mainPage->draw();

        const auto &appCtx = GAppContext::getInstance();
        const auto &settings = appCtx.getSubSystem<Config::Settings>();

        if (settings->getShowStatsWindow()) {
            UI::drawStats(m_currentFps);
        }

        UI::end();

        m_vkCore->renderToSwapchain([](VkCommandBuffer cmdBuffer) {
            ImDrawData *drawData = ImGui::GetDrawData();
            ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
        });
    }

    void UISubSystem::onUpdate(TimeMs dt) {
        m_mainPage->update(dt);
        m_currentFps = static_cast<int>(std::round(1000.0 / dt.count()));
    }

} // namespace Bess
