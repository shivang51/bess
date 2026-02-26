#include "application.h"
#include "common/types.h"
#include "application/application_state.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "events/application_event.h"
#include "imgui_impl_vulkan.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "plugin_manager.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "vulkan_core.h"
#include <chrono>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "application/window.h"
#include "common/bind_helpers.h"
#include "settings/settings.h"

namespace Bess {
    Application::Application() = default;

    Application::~Application() {
        shutdown();
    }

    void Application::draw() {
        auto &vkCore = Bess::Vulkan::VulkanCore::instance();
        if (m_mainWindow->wasWindowResized()) {
            m_mainWindow->resetWindowResizedFlag();
            const VkExtent2D newExtent = m_mainWindow->getExtent();
            vkCore.recreateSwapchain(newExtent);
        }

        vkCore.beginFrame();
        UI::begin();

        ApplicationState::getCurrentPage()->draw();

        if (Config::Settings::instance().getShowStatsWindow()) {
            UI::drawStats(m_currentFps);
        }

        UI::end();

        vkCore.renderToSwapchain(
            [](VkCommandBuffer cmdBuffer) {
                ImDrawData *drawData = ImGui::GetDrawData();
                ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
            });
        vkCore.endFrame();
    }

    void Application::run() {
        auto previousTime = std::chrono::steady_clock::now();

        TimeMs accumulatedTime(0.0);

        BESS_ASSERT(ApplicationState::getCurrentPage(),
                    "Current page of application is not set");

        while (!m_mainWindow->isClosed()) {
            auto currentTime = std::chrono::steady_clock::now();
            TimeMs deltaTime = currentTime - previousTime;
            previousTime = currentTime;

            accumulatedTime += deltaTime;

            const auto &frameTS = Config::Settings::instance().getFrameTimeStep();
            if (accumulatedTime < frameTS) {
                std::this_thread::sleep_for(frameTS - accumulatedTime);
                accumulatedTime += frameTS - accumulatedTime;
            }

            update(accumulatedTime);
            draw();
            m_currentFps = static_cast<int>(std::round(1000.0 / accumulatedTime.count()));
            accumulatedTime = std::chrono::duration<double>(0.0);
            Window::pollEvents();
        }
    }

    void Application::update(TimeMs ts) {
        EventSystem::EventDispatcher::instance().dispatchAll();
        m_mainWindow->update();
        ApplicationState::getCurrentPage()->update(ts, m_events);
        m_events.clear();
    }

    void Application::quit() const { m_mainWindow->close(); }

    // callbacks
    void Application::onWindowResize(int w, int h) {
        // Only handle resize if window is not minimized and size is reasonable
        if (w > 0 && h > 0) {
            const VkExtent2D newExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
            Bess::Vulkan::VulkanCore::instance().recreateSwapchain(newExtent);
        }

        ApplicationEvent::WindowResizeData data(w, h);
        ApplicationEvent event(ApplicationEventType::WindowResize, data);
        m_events.emplace_back(event);
    }

    void Application::onMouseWheel(double x, double y) {
        ApplicationEvent::MouseWheelData data(x, y);
        ApplicationEvent event(ApplicationEventType::MouseWheel, data);
        m_events.emplace_back(event);
    }

    void Application::onKeyPress(int key) {
        ApplicationEvent::KeyPressData data(key);
        ApplicationEvent event(ApplicationEventType::KeyPress, data);
        m_events.emplace_back(event);
    }

    void Application::onKeyRelease(int key) {
        ApplicationEvent::KeyReleaseData data(key);
        ApplicationEvent event(ApplicationEventType::KeyRelease, data);
        m_events.emplace_back(event);
    }

    void Application::onMouseButton(MouseButton button, MouseButtonAction action) {
        ApplicationEvent::MouseButtonData data(button, action);
        ApplicationEvent event(ApplicationEventType::MouseButton, data);
        m_events.emplace_back(event);
    }

    void Application::onMouseMove(double x, double y) {
        ApplicationEvent::MouseMoveData data(x, y);
        ApplicationEvent event(ApplicationEventType::MouseMove, data);
        m_events.emplace_back(event);
    }

    void Application::init(const std::string &path, AppStartupFlags flags) {
#ifdef DISABLE_PLUGINS
        flags |= AppStartupFlag::disablePlugins;
#endif
        BESS_INFO("[Application] Initializing application, with project path: {}", path.empty() ? "None" : path);

        auto &settings = Config::Settings::instance();
        settings.init();

        auto &pluginMangaer = Plugins::PluginManager::getInstance();

        if (flags & AppStartupFlag::disablePlugins) {
            BESS_WARN("[Application] Plugin support is disabled");
        } else {
            pluginMangaer.loadPluginsFromDirectory("plugins");
        }

        m_mainWindow = std::make_shared<Window>(800, 600, "Bess");
        ApplicationState::setParentWindow(m_mainWindow);

        m_mainWindow->onWindowResize(BIND_FN_L(Application::onWindowResize));
        m_mainWindow->onMouseWheel(BIND_FN_L(Application::onMouseWheel));
        m_mainWindow->onKeyPress(BIND_FN_L(Application::onKeyPress));
        m_mainWindow->onKeyRelease(BIND_FN_L(Application::onKeyRelease));
        m_mainWindow->onMouseButton(BIND_FN_L(Application::onMouseButton));
        m_mainWindow->onMouseMove(BIND_FN_L(Application::onMouseMove));

        const auto extensions = m_mainWindow->getVulkanExtensions();
        const VkExtent2D extent = m_mainWindow->getExtent();

        auto createSurface = [this](VkInstance &instance, VkSurfaceKHR &surface) {
            m_mainWindow->createWindowSurface(instance, surface);
        };

        auto &instance = Bess::Vulkan::VulkanCore::instance();
        instance.init(extensions, createSurface, extent);

        UI::init(m_mainWindow->getGLFWHandle());

        const auto page = Pages::MainPage::getInstance(ApplicationState::getParentWindow());

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

        m_mainWindow->close();
        m_mainWindow.reset();

        SimEngine::SimulationEngine::instance().destroy();
        Config::Settings::instance().cleanup();

        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.destroy();

        BESS_INFO("[Application] Application shutdown complete");
    }

    void Application::loadProject(const std::string &path) const {
        Pages::MainPage::getInstance()->getState().loadProject(path);
    }

    void Application::saveProject() const { Pages::MainPage::getInstance()->getState().saveCurrentProject(); }
} // namespace Bess
