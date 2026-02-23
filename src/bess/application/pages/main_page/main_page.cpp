#include "pages/main_page/main_page.h"
#include "asset_manager/asset_manager.h"
#include "component_catalog.h"
#include "events/application_event.h"
#include "macro_command.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/scene_components/conn_joint_scene_component.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <GLFW/glfw3.h>
#include <memory>
#include <ranges>

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

        // TODO(shivang): Think about a better way and scalabilty for plugins
        Canvas::NonSimSceneComponent::registerComponent<Canvas::TextComponent>("Text Component");

        REG_TO_SER_REGISTRY(Canvas::ConnJointSceneComp);
        REG_TO_SER_REGISTRY(Canvas::ConnectionSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::GroupSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::InputSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::NonSimSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SimulationSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::TextComponent);
    }

    MainPage::~MainPage() {
        destory();
    }

    void MainPage::destory() {
        if (m_isDestroyed)
            return;
        BESS_INFO("[MainPage] Destroying");
        m_state.getCommandSystem().reset();
        m_copiedComponents.clear();
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

    void MainPage::update(TFrameTime ts, std::vector<ApplicationEvent> &events) {
        const auto &viewportSize = UI::UIMain::state.mainViewport.getViewportSize();
        const auto &viewportPos = UI::UIMain::state.mainViewport.getViewportPos();

        m_state.update();

        m_state.getSceneDriver().getActiveScene()->updateViewportTransform({viewportPos, viewportSize});

        if (m_state.getSceneDriver().getActiveScene()->getSize() != viewportSize) {
            m_state.getSceneDriver().getActiveScene()->resize(viewportSize);
        }

        const bool imguiWantsKeyboard = ImGui::GetIO().WantTextInput;

        bool isDoubleClick = false;

        for (const auto &event : events) {
            switch (event.getType()) {
            case Bess::ApplicationEventType::MouseButton: {
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (m_lastMouseButtonEvent.processed && data.action == MouseButtonAction::press) {
                    m_lastMouseButtonEvent.timestamp = std::chrono::steady_clock::now();
                    m_lastMouseButtonEvent.data = data;
                    m_lastMouseButtonEvent.processed = false;
                } else {
                    if (data.action == m_lastMouseButtonEvent.data.action) {
                        const auto diff = std::chrono::steady_clock::now() - m_lastMouseButtonEvent.timestamp;
                        if (diff < std::chrono::milliseconds(500) &&
                            data.action == MouseButtonAction::press &&
                            m_lastMouseButtonEvent.data.button == data.button) {
                            isDoubleClick = true;
                        }
                        m_lastMouseButtonEvent.processed = true;
                    }
                }
            } break;
            case ApplicationEventType::KeyPress: {
                const auto data = event.getData<ApplicationEvent::KeyPressData>();
                m_state.setKeyPressed(data.key);
                m_state.setKeyDown(data.key, true);
            } break;
            case ApplicationEventType::KeyRelease: {
                const auto data = event.getData<ApplicationEvent::KeyReleaseData>();
                m_state.setKeyReleased(data.key);
                m_state.setKeyDown(data.key, false);
            } break;
            default:
                break;
            }
        }

        if (isDoubleClick) {
            auto data = m_lastMouseButtonEvent.data;
            data.action = MouseButtonAction::doubleClick;
            ApplicationEvent event(ApplicationEventType::MouseButton, data);
            events.emplace_back(event);
        }

        if (!imguiWantsKeyboard)
            handleKeyboardShortcuts();

        const auto isViewportHovered = UI::UIMain::state.mainViewport.isHovered();
        if (isViewportHovered)
            m_state.getSceneDriver().update(ts, events);
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    void MainPage::handleKeyboardShortcuts() {
        const bool ctrlPressed = m_state.isKeyDown(GLFW_KEY_LEFT_CONTROL) ||
                                 m_state.isKeyDown(GLFW_KEY_RIGHT_CONTROL);

        const bool shiftPressed = m_state.isKeyDown(GLFW_KEY_LEFT_SHIFT) ||
                                  m_state.isKeyDown(GLFW_KEY_RIGHT_SHIFT);

        if (ctrlPressed) {
            if (m_state.isKeyPressed(GLFW_KEY_S)) {
                m_state.actionFlags.saveProject = true;
            } else if (m_state.isKeyPressed(GLFW_KEY_O)) {
                m_state.actionFlags.openProject = true;
            } else if (m_state.isKeyPressed(GLFW_KEY_Z)) {
                if (shiftPressed) {
                    m_state.getCommandSystem().redo();
                } else {
                    m_state.getCommandSystem().undo();
                }
            } else if (m_state.isKeyPressed(GLFW_KEY_G)) {
                UI::ProjectExplorer::groupSelectedNodes();
            } else if (m_state.isKeyPressed(GLFW_KEY_A)) {
                m_state.getSceneDriver()->selectAllEntities();
            } else if (m_state.isKeyPressed(GLFW_KEY_C)) {
                copySelectedEntities();
            } else if (m_state.isKeyPressed(GLFW_KEY_V)) {
                pasteCopiedEntities();
            }
        } else if (shiftPressed) {
            if (m_state.isKeyPressed(GLFW_KEY_A)) {
                UI::ComponentExplorer::isShown = !UI::ComponentExplorer::isShown;
            }
        } else {
            if (m_state.isKeyPressed(GLFW_KEY_DELETE)) {
                auto ids = m_state.getSceneDriver()->getState().getSelectedComponents() |
                           std::ranges::views::keys |
                           std::ranges::to<std::vector<UUID>>();

                m_state.getCommandSystem().execute(std::make_unique<Cmd::DeleteCompCmd>(ids));
            } else if (m_state.isKeyPressed(GLFW_KEY_F)) {
                m_state.getSceneDriver()->focusCameraOnSelected();
            } else if (m_state.isKeyPressed(GLFW_KEY_TAB)) {
                m_state.getSceneDriver()->toggleSchematicView();
            } else if (m_state.isKeyPressed(GLFW_KEY_ESCAPE)) {
                if (UI::ComponentExplorer::isShown) {
                    UI::ComponentExplorer::isShown = false;
                }
            }
        }
    }

    MainPageState &MainPage::getState() {
        return m_state;
    };

    void MainPage::copySelectedEntities() {
        m_copiedComponents.clear();

        auto &simEngine = SimEngine::SimulationEngine::instance();
        const auto &catalogInstance = SimEngine::ComponentCatalog::instance();

        const auto &sceneState = m_state.getSceneDriver()->getState();
        const auto &selComponents = sceneState.getSelectedComponents() | std::views::keys;

        for (const auto &selId : selComponents) {
            const auto comp = sceneState.getComponentByUuid(selId);
            const bool isSimComp = comp->getType() == Canvas::SceneComponentType::simulation;
            const bool isNonSimComp = comp->getType() == Canvas::SceneComponentType::nonSimulation;
            CopiedComponent compData{};
            if (isSimComp) {
                const auto casted = comp->cast<Canvas::SimulationSceneComponent>();
                compData.def = simEngine.getComponentDefinition(casted->getSimEngineId());
            } else if (isNonSimComp) {
                const auto casted = comp->cast<Canvas::NonSimSceneComponent>();
                compData.nsComp = casted->getTypeIndex();
            } else {
                continue;
            }
            compData.pos = comp->getTransform().position;
            m_copiedComponents.emplace_back(compData);
        }
    }

    void MainPage::pasteCopiedEntities() {
        auto &cmdSystem = m_state.getCommandSystem();
        auto &scene = m_state.getSceneDriver();

        auto macroCmd = std::make_unique<Cmd::MacroCommand>();

        for (auto &comp : m_copiedComponents) {
            const auto pos = comp.pos + glm::vec2(50.f, 50.f);
            if (comp.nsComp == typeid(void)) {
                auto components = Canvas::SimulationSceneComponent::createNewAndRegister(comp.def);
                auto sceneComp = components.front()->template cast<Canvas::SimulationSceneComponent>();
                components.erase(components.begin());
                sceneComp->setCompDef(comp.def->clone());
                sceneComp->getTransform().position.x = pos.x;
                sceneComp->getTransform().position.y = pos.y;

                if (scene->hasPluginDrawHookForComponentHash(comp.def->getHash())) {
                    auto hook = scene->getPluginDrawHookForComponentHash(comp.def->getHash());
                    sceneComp->cast<Canvas::SimulationSceneComponent>()->setDrawHook(hook);
                }
                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::SimulationSceneComponent>>(sceneComp, components);
                macroCmd->addCommand(std::move(addCmd));
            } else {
                auto inst = Canvas::NonSimSceneComponent::getInstance(comp.nsComp);
                inst->getTransform().position.x = pos.x;
                inst->getTransform().position.y = pos.y;
                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::NonSimSceneComponent>>(inst);
                macroCmd->addCommand(std::move(addCmd));
            }
        }

        cmdSystem.execute(std::move(macroCmd));
    }
} // namespace Bess::Pages
