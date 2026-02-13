#include "pages/main_page/main_page.h"
#include "application/types.h"
#include "asset_manager/asset_manager.h"
#include "events/application_event.h"
#include "pages/main_page/main_page_state.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <GLFW/glfw3.h>
#include <memory>

namespace Bess::Pages {
    std::shared_ptr<Page> MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    std::shared_ptr<MainPage> MainPage::getTypedInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = getInstance(parentWindow);
        return std::dynamic_pointer_cast<MainPage>(instance);
    }

    MainPage::MainPage(const std::shared_ptr<Window> &parentWindow) {
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

        m_state.getSceneDriver().createDefaultScene();

        UI::UIMain::setViewportTexture(m_state.getSceneDriver()->getTextureId());

        m_state.initCmdSystem(m_state.getSceneDriver().getActiveScene().get(),
                              &SimEngine::SimulationEngine::instance());

        m_state.createNewProject(false);
    }

    MainPage::~MainPage() {
        destory();
    }

    void MainPage::destory() {
        if (m_isDestroyed)
            return;
        BESS_INFO("[MainPage] Destroying");
        m_state.getCommandSystem().reset();
        auto &instance = Bess::Vulkan::VulkanCore::instance();
        instance.cleanup([&]() {
            m_state.getSceneDriver()->destroy();
            Assets::AssetManager::instance().clear();
            UI::vulkanCleanup(instance.getDevice());
        });
        BESS_INFO("[MainPage] Destroyed");
        m_isDestroyed = true;
    }

    void MainPage::draw() {
        m_state.getSceneDriver().render();
        UI::UIMain::draw();
    }

    void MainPage::update(TFrameTime ts, const std::vector<ApplicationEvent> &events) {
        const auto &viewportSize = UI::UIMain::state.mainViewport.getViewportSize();
        const auto &viewportPos = UI::UIMain::state.mainViewport.getViewportPos();

        m_state.getSceneDriver().getActiveScene()->updateViewportTransform({viewportPos, viewportSize});

        if (m_state.getSceneDriver().getActiveScene()->getSize() != viewportSize) {
            m_state.getSceneDriver().getActiveScene()->resize(viewportSize);
        }

        m_state.releasedKeysFrame.clear();
        m_state.pressedKeysFrame.clear();

        for (const auto &event : events) {
            switch (event.getType()) {
            case ApplicationEventType::KeyPress: {
                const auto data = event.getData<ApplicationEvent::KeyPressData>();
                m_state.setKeyPressed(data.key, true);
                m_state.pressedKeysFrame[data.key] = true;
            } break;
            case ApplicationEventType::KeyRelease: {
                const auto data = event.getData<ApplicationEvent::KeyReleaseData>();
                m_state.setKeyPressed(data.key, false);
                m_state.releasedKeysFrame[data.key] = true;
            } break;
            default:
                break;
            }
        }

        handleKeyboardShortcuts();

        const auto isViewportHovered = UI::UIMain::state.mainViewport.isHovered();
        if (isViewportHovered)
            m_state.getSceneDriver().update(ts, events);
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    void MainPage::handleKeyboardShortcuts() {
        const bool ctrlPressed = m_state.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                                 m_state.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

        const bool shiftPressed = m_state.isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
                                  m_state.isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

        if (ctrlPressed) {
            if (m_state.releasedKeysFrame[GLFW_KEY_S]) {
                m_state.actionFlags.saveProject = true;
            } else if (m_state.releasedKeysFrame[GLFW_KEY_O]) {
                m_state.actionFlags.openProject = true;
            } else if (m_state.pressedKeysFrame[GLFW_KEY_Z]) {
                if (shiftPressed) {
                    m_state.getCommandSystem().redo();
                } else {
                    m_state.getCommandSystem().undo();
                }
            } else if (m_state.releasedKeysFrame[GLFW_KEY_G]) {
                UI::ProjectExplorer::groupSelectedNodes();
            } else if (m_state.releasedKeysFrame[GLFW_KEY_A]) {
                m_state.getSceneDriver()->selectAllEntities();
            } else if (m_state.releasedKeysFrame[GLFW_KEY_C]) {
                m_state.getSceneDriver()->copySelectedComponents();
            } else if (m_state.releasedKeysFrame[GLFW_KEY_V]) {
                m_state.getSceneDriver()->generateCopiedComponents();
            }
        } else if (shiftPressed) {
            if (m_state.pressedKeysFrame[GLFW_KEY_A]) {
                UI::ComponentExplorer::isShown = !UI::ComponentExplorer::isShown;
            }
        } else {
            if (m_state.pressedKeysFrame[GLFW_KEY_DELETE]) {
                m_state.getSceneDriver()->deleteSelectedSceneEntities();
            } else if (m_state.pressedKeysFrame[GLFW_KEY_F]) {
                m_state.getSceneDriver()->focusCameraOnSelected();
            } else if (m_state.pressedKeysFrame[GLFW_KEY_TAB]) {
                m_state.getSceneDriver()->toggleSchematicView();
            } else if (m_state.pressedKeysFrame[GLFW_KEY_ESCAPE]) {
                if (UI::ComponentExplorer::isShown) {
                    UI::ComponentExplorer::isShown = false;
                }
            }
        }
    }

    MainPageState &MainPage::getState() {
        return m_state;
    };
} // namespace Bess::Pages
