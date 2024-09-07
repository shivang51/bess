#include "application.h"
#include "application_state.h"
#include "events/application_event.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "pages/start_page/start_page.h"
#include "renderer/renderer.h"
#include "ui/ui.h"
#include <GLFW/glfw3.h>

#include "components_manager/component_bank.h"
#include "components_manager/components_manager.h"

#include "components/clock.h"
#include "settings/settings.h"

#include "common/bind_helpers.h"
#include "window.h"

using Bess::Renderer2D::Renderer;

namespace Bess {
    Application::Application() : m_mainWindow(800, 600, "Bess") {
        init();
        Pages::StartPage::getInstance()->show();
    }

    Application::Application(const std::string &path) : m_mainWindow(800, 600, "Bess") {
        init();
        loadProject(path);
        Pages::MainPage::getInstance(ApplicationState::getParentWindow())->show();
    }

    Application::~Application() {
        UI::shutdown();
        shutdown();
    }

    static int fps = 0;

    void Application::draw() {
        UI::begin();
        ApplicationState::getCurrentPage()->draw();
        // UI::UIMain::drawStats(fps);
        UI::end();
    }

    void Application::run() {
        double prevTime = glfwGetTime();
        double frameTime = 1.0 / 60.0;
        double accumulatedTime = 0.0;

        while (!m_mainWindow.isClosed()) {
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
        m_mainWindow.update();
        ApplicationState::getCurrentPage()->update(m_events);
        m_events.clear();
    }

    void Application::quit() { m_mainWindow.close(); }

    // callbacks

    void Application::onWindowResize(int w, int h) {
        // glViewport(0, 0, w, h);
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

    void Application::init() {
        ApplicationState::setParentWindow(&m_mainWindow);

        Config::Settings::init();

        Simulator::ComponentsManager::init();

        Simulator::ComponentBankElement el(Simulator::ComponentType::inputProbe, "Input Probe");
        Simulator::ComponentBank::addToCollection("I/O", el);
        Simulator::ComponentBank::addToCollection("I/O", {Simulator::ComponentType::outputProbe, "Ouput Probe"});
        Simulator::ComponentBank::addToCollection("I/O", {Simulator::ComponentType::clock, "Clock"});
        Simulator::ComponentBank::addToCollection("Misc", {Simulator::ComponentType::text, "Text"});
        Simulator::ComponentBank::loadMultiFromJson("assets/comp_collections.json");

        UI::init(m_mainWindow.getGLFWHandle());

        Renderer::init();

        m_mainWindow.onWindowResize(BIND_FN_2(Application::onWindowResize));
        m_mainWindow.onMouseWheel(BIND_FN_2(Application::onMouseWheel));
        m_mainWindow.onKeyPress(BIND_FN_1(Application::onKeyPress));
        m_mainWindow.onKeyRelease(BIND_FN_1(Application::onKeyRelease));
        m_mainWindow.onLeftMouse(BIND_FN_1(Application::onLeftMouse));
        m_mainWindow.onRightMouse(BIND_FN_1(Application::onRightMouse));
        m_mainWindow.onMiddleMouse(BIND_FN_1(Application::onMiddleMouse));
        m_mainWindow.onMouseMove(BIND_FN_2(Application::onMouseMove));
    }

    void Application::shutdown() { m_mainWindow.close(); }

    void Application::loadProject(const std::string &path) {
        Pages::MainPageState::getInstance()->loadProject(path);
    }

    void Application::saveProject() { Pages::MainPageState::getInstance()->saveCurrentProject(); }

} // namespace Bess
