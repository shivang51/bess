#include "application.h"
#include "application/application_state.h"
#include "common/bess_assert.h"
#include "common/g_app_context.h"
#include "common/logger.h"
#include "common/types.h"
#include "event_dispatcher.h"
#include "imgui_impl_vulkan.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "services/plugin_service/plugin_service.h"
#include "simulation_engine.h"
#include "sub_systems/input_sub_system.h"
#include "ui/ui.h"
#include "vulkan_core.h"
#include <chrono>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "application/window.h"
#include "settings/settings.h"

namespace Bess {
    Application::Application() = default;

    Application::~Application() { shutdown(); }

    void Application::draw() {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto vkCore = appCtx.getSubSystem<Bess::Vulkan::VulkanCore>();
        if (m_mainWindow->wasWindowResized()) {
            m_mainWindow->resetWindowResizedFlag();
            const VkExtent2D newExtent = m_mainWindow->getExtent();
            vkCore->recreateSwapchain(newExtent);
        }

        vkCore->beginFrame();
        UI::begin();

        ApplicationState::getCurrentPage()->draw();

        if (Config::Settings::instance().getShowStatsWindow()) {
            UI::drawStats(m_currentFps);
        }

        UI::end();

        vkCore->renderToSwapchain([](VkCommandBuffer cmdBuffer) {
            ImDrawData *drawData = ImGui::GetDrawData();
            ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
        });
        vkCore->endFrame();
    }

    void Application::run() {
        BESS_ASSERT(ApplicationState::getCurrentPage(),
                    "Current page of application is not set");

        auto previousTime = std::chrono::steady_clock::now();

        TimeMs accumulatedTime(0.0);

        auto &appCtx = GAppContext::getInstance();

        while (!m_mainWindow->isClosed()) {
            auto currentTime = std::chrono::steady_clock::now();
            TimeMs deltaTime = currentTime - previousTime;
            previousTime = currentTime;

            accumulatedTime += deltaTime;

            const auto &frameTS =
                Config::Settings::instance().getFrameTimeStep();
            if (accumulatedTime < frameTS) {
                std::this_thread::sleep_for(frameTS - accumulatedTime);
                accumulatedTime += frameTS - accumulatedTime;
            }

            appCtx.beginFrame();
            Window::pollEvents();
            update(accumulatedTime);
            draw();

            m_currentFps =
                static_cast<int>(std::round(1000.0 / accumulatedTime.count()));
            accumulatedTime = std::chrono::duration<double>(0.0);
        }
    }

    void Application::update(TimeMs ts) {

        GAppContext::getInstance().update(ts);

        ApplicationState::getCurrentPage()->update(ts);
    }

    void Application::quit() const { m_mainWindow->close(); }

    void Application::init(const std::string &path, AppStartupFlags flags) {
#ifdef DISABLE_PLUGINS
        flags |= AppStartupFlag::disablePlugins;
#endif
        BESS_INFO(
            "[Application] Initializing application, with project path: {}",
            path.empty() ? "None" : path);

        auto &settings = Config::Settings::instance();
        settings.init();

        if (flags & AppStartupFlag::disablePlugins) {
            BESS_WARN("[Application] Plugin support is disabled");
        } else {
            Svc::PluginService::getInstance().init();
        }

        auto &appCtx = GAppContext::getInstance();

        m_mainWindow = appCtx.addSubSystem<Window>(800, 660, "Bess");

        appCtx.addSubSystem<InputSubSystem>();
        appCtx.addSubSystem<VulkanCore>();
        appCtx.addSubSystem<EventSystem::EventDispatcher>();

        appCtx.init();

        ApplicationState::setParentWindow(m_mainWindow);

        const auto extensions = m_mainWindow->getVulkanExtensions();
        const VkExtent2D extent = m_mainWindow->getExtent();

        auto createSurface = [this](VkInstance &instance,
                                    VkSurfaceKHR &surface) {
            m_mainWindow->createWindowSurface(instance, surface);
        };

        UI::init(m_mainWindow->getGLFWHandle());

        const auto page =
            Pages::MainPage::getInstance(ApplicationState::getParentWindow());

        ApplicationState::setCurrentPage(page);

        if (!path.empty())
            loadProject(path);

        BESS_INFO("[Application] Application initialized successfully\n");
    }

    void Application::shutdown() {
        BESS_INFO("[Application] Shutting down application");

        ApplicationState::setCurrentPage(nullptr);
        Pages::MainPage::getInstance()->destory();
        Pages::MainPage::getInstance().reset();

        UI::shutdown();

        ApplicationState::clear();

        // TODO: move simulation_engine etc inside it
        GAppContext::getInstance().destroy();

        SimEngine::SimulationEngine::instance().destroy();
        Config::Settings::instance().cleanup();

        if (Svc::PluginService::getInstance().getIsInitialized()) {
            Svc::PluginService::getInstance().destroy();
        }

        BESS_INFO("[Application] Application shutdown complete");
    }

    void Application::loadProject(const std::string &path) const {
        Pages::MainPage::getInstance()->getState().loadProject(path);
    }

    void Application::saveProject() const {
        Pages::MainPage::getInstance()->getState().saveCurrentProject();
    }
} // namespace Bess
