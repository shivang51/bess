#include "application.h"
#include "application_state.h"
#include "common/log.h"
#include "events/application_event.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "ui/ui.h"

#include "common/bind_helpers.h"
#include "settings/settings.h"
#include "window.h"

namespace Bess {
    Application::~Application() {
        UI::shutdown();
        shutdown();
    }

    static int fps = 0;

    void Application::draw() {
        UI::begin();
        ApplicationState::getCurrentPage()->draw();
        // UI::drawStats(fps);
        UI::end();
    }

    void Application::run() {
        double prevTime = glfwGetTime();
        double frameTime = 1.0 / 60.0;
        double accumulatedTime = 0.0;

        while (!m_mainWindow->isClosed()) {
            double currTime = glfwGetTime();
            double deltaTime = currTime - prevTime;
            prevTime = currTime;

            accumulatedTime += deltaTime;

            if (accumulatedTime >= frameTime) {
                update();
                draw();
                fps = static_cast<int>(std::round(1.0 / accumulatedTime));
                accumulatedTime = 0.0;
            }

            // Poll events
            Window::pollEvents();
        }
    }

    void Application::update() {
        m_mainWindow->update();
        ApplicationState::getCurrentPage()->update(m_events);
        m_events.clear();
    }

    void Application::quit() { m_mainWindow->close(); }

    // callbacks
    void Application::onWindowResize(int w, int h) {
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

    void Application::init(const std::string &path) {
        SimEngine::Logger::getInstance().initLogger(BESS_LOGGER_NAME);
        m_mainWindow = std::make_shared<Window>(800, 600, "Bess");
        ApplicationState::setParentWindow(m_mainWindow);

        Config::Settings::init();
        UI::init(m_mainWindow->getGLFWHandle());

        m_mainWindow->onWindowResize(BIND_FN_2(Application::onWindowResize));
        m_mainWindow->onMouseWheel(BIND_FN_2(Application::onMouseWheel));
        m_mainWindow->onKeyPress(BIND_FN_1(Application::onKeyPress));
        m_mainWindow->onKeyRelease(BIND_FN_1(Application::onKeyRelease));
        m_mainWindow->onLeftMouse(BIND_FN_1(Application::onLeftMouse));
        m_mainWindow->onRightMouse(BIND_FN_1(Application::onRightMouse));
        m_mainWindow->onMiddleMouse(BIND_FN_1(Application::onMiddleMouse));
        m_mainWindow->onMouseMove(BIND_FN_2(Application::onMouseMove));

        Pages::MainPage::getInstance(ApplicationState::getParentWindow())->show();

        if (!path.empty())
            loadProject(path);
    }

    void Application::shutdown() { m_mainWindow->close(); }

    void Application::loadProject(const std::string &path) {
        Pages::MainPageState::getInstance()->loadProject(path);
    }

    void Application::saveProject() { Pages::MainPageState::getInstance()->saveCurrentProject(); }

} // namespace Bess
