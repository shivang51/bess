#include "ui_sub_system.h"
#include "bess_core/g_app_context.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "pages/main_page/main_page.h"
#include "settings/settings.h"
#include "sub_systems/renderer_context.h"
#include "vulkan_core.h"
#include "ui.h"

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
        m_mainPage->draw();

        const auto &appCtx = GAppContext::getInstance();
        const auto &settings = appCtx.getSubSystem<Config::Settings>();

        if (settings->getShowStatsWindow()) {
            UI::drawStats(m_currentFps);
        }
    }

    void UISubSystem::onUpdate(TimeMs dt) {
        m_mainPage->update(dt);
        m_currentFps = static_cast<int>(std::round(1000.0 / dt.count()));
    }

} // namespace Bess
