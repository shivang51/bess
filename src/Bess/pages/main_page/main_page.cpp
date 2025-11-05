#include "pages/main_page/main_page.h"
#include "asset_manager/asset_manager.h"
#include "events/application_event.h"
#include "pages/page_identifier.h"
#include "scene/scene.h"
#include "scene/scene_pch.h"
#include "simulation_engine.h"
#include "application/types.h"
#include "ui/ui.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <memory>

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    std::shared_ptr<MainPage> MainPage::getTypedInstance(std::shared_ptr<Window> parentWindow) {
        static auto instance = getInstance(parentWindow);
        return std::dynamic_pointer_cast<MainPage>(instance);
    }

    MainPage::MainPage(std::shared_ptr<Window> parentWindow) : Page(PageIdentifier::MainPage) {
        if (m_parentWindow == nullptr && parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need to pass a parent window.");
        }
        m_parentWindow = parentWindow;

        SimEngine::SimulationEngine::instance();

        const auto extensions = m_parentWindow->getVulkanExtensions();
        const VkExtent2D extent = m_parentWindow->getExtent();

        auto createSurface = [parentWindow](VkInstance &instance, VkSurfaceKHR &surface) {
            parentWindow->createWindowSurface(instance, surface);
        };

        auto &instance = Bess::Vulkan::VulkanCore::instance();
        instance.init(extensions, createSurface, extent);

        m_state = MainPageState::getInstance();

        m_scene = Canvas::Scene::instance();
        UI::UIMain::setViewportTexture(m_scene->getTextureId());
    }

    MainPage::~MainPage() {
        destory();
    }

    void MainPage::destory() {
        if (m_isDestroyed)
            return;
        BESS_INFO("[MainPage] Destroying");
        auto &instance = Bess::Vulkan::VulkanCore::instance();
        instance.cleanup([&]() {
            m_scene->destroy();
            Assets::AssetManager::instance().clear();
            UI::vulkanCleanup(instance.getDevice());
        });
        BESS_INFO("[MainPage] Destroyed");
        m_isDestroyed = true;
    }

    void MainPage::draw() {
        m_scene->render();
    }

    void MainPage::update(TFrameTime ts, const std::vector<ApplicationEvent> &events) {
        if (m_scene->getSize() != UI::UIMain::state.viewportSize) {
            m_scene->resize(UI::UIMain::state.viewportSize);
        }

        for (const auto &event : events) {
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

        if (UI::UIMain::state.viewportEventFlag)
            m_scene->update(ts, events);
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    std::shared_ptr<Canvas::Scene> MainPage::getScene() const {
        return m_scene;
    }
} // namespace Bess::Pages
