#include "pages/main_page/main_page.h"
#include "events/application_event.h"
#include "pages/page_identifier.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/ui_main/ui_main.h"
#include <memory>

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    std::shared_ptr<MainPage> MainPage::getTypedInstance(std::shared_ptr<Window> parentWindow) {
        const auto instance = getInstance(parentWindow);
        return std::dynamic_pointer_cast<MainPage>(instance);
    }

    MainPage::MainPage(std::shared_ptr<Window> parentWindow) : Page(PageIdentifier::MainPage) {
        if (m_parentWindow == nullptr && parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need to pass a parent window.");
        }
        m_parentWindow = parentWindow;

        SimEngine::SimulationEngine::instance();

        UI::UIMain::state.viewportTexture = m_scene.getTextureId();
        m_state = MainPageState::getInstance();
    }

    void MainPage::draw() {
        UI::UIMain::draw();
        m_scene.render();
    }

    void MainPage::update(const std::vector<ApplicationEvent> &events) {
        if (m_scene.getSize() != UI::UIMain::state.viewportSize) {
            m_scene.resize(UI::UIMain::state.viewportSize);
        }

        for (auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::KeyPress: {
                const auto data = event.getData<ApplicationEvent::KeyPressData>();
                m_state->setKeyPressed(data.key, true);
            } break;
            case ApplicationEventType::KeyRelease: {
                const auto data = event.getData<ApplicationEvent::KeyReleaseData>();
                m_state->setKeyPressed(data.key, false);
            } break;
            default:
                break;
            }
        }

        if (UI::UIMain::state.isViewportFocused)
            m_scene.update(events);
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    Canvas::Scene &MainPage::getScene() {
        return m_scene;
    }

} // namespace Bess::Pages
