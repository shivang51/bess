#include "application.h"
#include "application_state.h"
#include "common/log.h"
#include "events/application_event.h"
#include "imgui_impl_vulkan.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "plugin_manager.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "common/bind_helpers.h"
#include "settings/settings.h"
#include "window.h"

namespace Bess {
    Application::Application() {
        SimEngine::Logger::getInstance().initLogger(BESS_LOGGER_NAME);
    }

    Application::~Application() {
        shutdown();
    }

    static int fps = 0;

    void Application::draw() {
        ApplicationState::getCurrentPage()->draw();

        auto &vkCore = Bess::Vulkan::VulkanCore::instance();
        if (m_mainWindow->wasWindowResized()) {
            m_mainWindow->resetWindowResizedFlag();
            const VkExtent2D newExtent = m_mainWindow->getExtent();
            vkCore.recreateSwapchain(newExtent);
        }

        vkCore.beginFrame();
        UI::begin();

        UI::UIMain::draw();
        UI::drawStats(fps);
        UI::end();

        vkCore.renderToSwapchain(
            [](VkCommandBuffer cmdBuffer) {
                ImDrawData *drawData = ImGui::GetDrawData();
                ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer);
            });
        vkCore.endFrame();
    }

    void Application::run() {
        constexpr TFrameTime logicTimestep(1000.0f / 60.0f);

        m_clock = {};
        auto previousTime = m_clock.now();

        TFrameTime accumulatedTime(0.0);

        while (!m_mainWindow->isClosed()) {
            auto currentTime = m_clock.now();
            TFrameTime deltaTime = currentTime - previousTime;
            previousTime = currentTime;

            accumulatedTime += deltaTime;

            if (accumulatedTime < logicTimestep) {
                std::this_thread::sleep_for(logicTimestep - accumulatedTime);
                accumulatedTime += logicTimestep - accumulatedTime;
            }

            update(accumulatedTime);
            draw();
            fps = static_cast<int>(std::round(1000.0 / accumulatedTime.count()));
            accumulatedTime = std::chrono::duration<double>(0.0);
            Window::pollEvents();
        }
    }

    void Application::update(TFrameTime ts) {
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

    void Application::onLeftMouse(bool pressed) {
        ApplicationEvent::MouseButtonData data(MouseButton::left, pressed);
        ApplicationEvent event(ApplicationEventType::MouseButton, data);
        m_events.emplace_back(event);
    }

    void Application::onRightMouse(bool pressed) {
        ApplicationEvent::MouseButtonData data(MouseButton::right, pressed);
        ApplicationEvent event(ApplicationEventType::MouseButton, data);
        m_events.emplace_back(event);
    }

    void Application::onMiddleMouse(bool pressed) {
        ApplicationEvent::MouseButtonData data(MouseButton::middle, pressed);
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

        auto &pluginMangaer = Plugins::PluginManager::getInstance();

        if (flags & AppStartupFlag::disablePlugins) {
            BESS_WARN("[Application] Plugin support is disabled");
        } else {
            pluginMangaer.loadPluginsFromDirectory("plugins");
        }

        m_mainWindow = std::make_shared<Window>(800, 600, "Bess");
        ApplicationState::setParentWindow(m_mainWindow);

        Config::Settings::init();

        m_mainWindow->onWindowResize(BIND_FN_2(Application::onWindowResize));
        m_mainWindow->onMouseWheel(BIND_FN_2(Application::onMouseWheel));
        m_mainWindow->onKeyPress(BIND_FN_1(Application::onKeyPress));
        m_mainWindow->onKeyRelease(BIND_FN_1(Application::onKeyRelease));
        m_mainWindow->onLeftMouse(BIND_FN_1(Application::onLeftMouse));
        m_mainWindow->onRightMouse(BIND_FN_1(Application::onRightMouse));
        m_mainWindow->onMiddleMouse(BIND_FN_1(Application::onMiddleMouse));
        m_mainWindow->onMouseMove(BIND_FN_2(Application::onMouseMove));

        const auto page = Pages::MainPage::getInstance(ApplicationState::getParentWindow());
        UI::init(m_mainWindow->getGLFWHandle());

        page->show();

        if (!path.empty())
            loadProject(path);

        BESS_INFO("[Application] Application initialized successfully\n");
    }

    void Application::shutdown() const {
        BESS_INFO("[Application] Shutting down application");
        Pages::MainPage::getTypedInstance()->destory();
        UI::shutdown();
        ApplicationState::clear();
        m_mainWindow->close();
        SimEngine::SimulationEngine::instance().destroy();
        auto &pluginMangaer = Plugins::PluginManager::getInstance();
        pluginMangaer.destroy();
    }

    void Application::loadProject(const std::string &path) const {
        Pages::MainPageState::getInstance()->loadProject(path);
    }

    void Application::saveProject() const { Pages::MainPageState::getInstance()->saveCurrentProject(); }
} // namespace Bess
