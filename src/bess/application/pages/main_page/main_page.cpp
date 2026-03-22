#include "pages/main_page/main_page.h"
#include "asset_manager/asset_manager.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "component_catalog.h"
#include "events/application_event.h"
#include "geometric.hpp"
#include "macro_command.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "pages/main_page/cmds/module_comp_cmd.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/scene_components/conn_joint_scene_component.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/module_scene_component.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_probe_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "plugin_manager.h"
#include "scene_ser_reg.h"
#include "scene_state/components/scene_component_types.h"
#include "services/copy_paste_service.h"
#include "simulation_engine.h"
#include "ui/ui.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/ui_main.h"
#include "vulkan_core.h"
#include <GLFW/glfw3.h>
#include <chrono>
#include <functional>
#include <memory>
#include <ranges>
#include <unordered_set>

namespace Bess::Pages {
    std::shared_ptr<MainPage> &MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    MainPage::MainPage(const std::shared_ptr<Window> &parentWindow) {
        if (m_parentWindow == nullptr && parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need to pass a parent window.");
        }
        m_parentWindow = parentWindow;

        SimEngine::SimulationEngine::instance();

        // TODO(shivang): Think about a better way and scalabilty for plugins
        Canvas::NonSimSceneComponent::registerComponent<Canvas::TextComponent>("Text Component");
        Canvas::NonSimSceneComponent::registerComponent<Canvas::SlotProbeSceneComponent>("Probe");

        REG_TO_SER_REGISTRY(Canvas::ConnJointSceneComp);
        REG_TO_SER_REGISTRY(Canvas::ConnectionSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::GroupSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::InputSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::NonSimSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SimulationSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::TextComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotProbeSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::ModuleSceneComponent);

        UI::UIMain::init();

        auto scene = m_state.getSceneDriver().createNewScene();
        m_state.getSceneDriver().setRootSceneId(scene->getSceneId());

        m_state.getSceneDriver().setActiveScene(0, false); // false to avoid infinite loop of main page init

        m_state.initCmdSystem();
        m_state.getCommandSystem().setScene(scene.get());
        m_state.getCommandSystem().setSimEngine(&SimEngine::SimulationEngine::instance());

        m_state.createNewProject(false);

        Svc::SvcConnection::instance().init();
        Svc::CopyPaste::Context::instance().init();

        BESS_DEBUG("MainPage created successfully");
    }

    MainPage::~MainPage() {
        destory();
        BESS_DEBUG("MainPage died now");
    }

    void MainPage::destory() {
        if (m_isDestroyed)
            return;
        BESS_INFO("[MainPage] Destroying");

        Svc::CopyPaste::Context::instance().destroy();
        Svc::SvcConnection::instance().destroy();

        Canvas::NonSimSceneComponent::clearRegistry();
        Canvas::SceneSerReg::clearRegistry();

        m_state.getCommandSystem().reset();
        m_copiedComponents.clear();

        auto &instance = Bess::Vulkan::VulkanCore::instance();
        instance.cleanup([&]() {
            for (const auto &panel : UI::UIMain::getScenePanels()) {
                panel->destroyViewport();
            }
            m_state.getSceneDriver()->destroy();
            Assets::AssetManager::instance().clear();
            UI::vulkanCleanup(instance.getDevice());
        });

        UI::UIMain::destroy();

        BESS_INFO("[MainPage] Destroyed");
        m_isDestroyed = true;
    }

    void MainPage::draw() {
        UI::UIMain::draw();

        const auto &plugins = Plugins::PluginManager::getInstance().getLoadedPlugins();
        for (const auto &plugin : plugins) {
            plugin.second->drawUI();
        }
    }

    void MainPage::update(TimeMs ts, std::vector<ApplicationEvent> &events) {
        m_state.update();

        const bool imguiWantsKeyboard = ImGui::GetIO().WantTextInput;

        int clickEvtIdx = -1;

        int idx = -1;
        for (const auto &event : events) {
            idx++;

            switch (event.getType()) {
            case Bess::ApplicationEventType::MouseButton: {
                const auto data = event.getData<ApplicationEvent::MouseButtonData>();
                if (data.action != MouseButtonAction::press) {
                    continue;
                }

                const bool isSameBtn = data.button == m_lastMouseButtonEvent.data.button;
                const float dis = glm::distance(data.pos, m_lastMouseButtonEvent.data.pos);
                const auto timeDif = std::chrono::steady_clock::now() - m_lastMouseButtonEvent.timestamp;

                if (isSameBtn && dis <= 5.f && timeDif < TimeMs(500)) {
                    m_clickCount++;
                } else {
                    m_clickCount = 1;
                }
                clickEvtIdx = idx;

                m_lastMouseButtonEvent.timestamp = std::chrono::steady_clock::now();
                m_lastMouseButtonEvent.data = data;
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

        if (m_clickCount == 2) {
            BESS_ASSERT(clickEvtIdx != -1, "Click event idx can't be -1, when double click is valid");
            events.erase(events.begin() + clickEvtIdx);
            auto data = m_lastMouseButtonEvent.data;
            data.action = MouseButtonAction::doubleClick;
            ApplicationEvent event(ApplicationEventType::MouseButton, data);
            events.emplace_back(event);
            m_clickCount = 0;
        }

        if (!imguiWantsKeyboard)
            handleKeyboardShortcuts();

        UI::UIMain::update(ts, events);
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
                UI::UIMain::getPanel<UI::ProjectExplorer>()->groupSelectedNodes();
            } else if (m_state.isKeyPressed(GLFW_KEY_A)) {
                m_state.getSceneDriver()->selectAllEntities();
            } else if (m_state.isKeyPressed(GLFW_KEY_C)) {
                copySelectedEntities();
            } else if (m_state.isKeyPressed(GLFW_KEY_V)) {
                pasteCopiedEntities();
            }
        } else if (shiftPressed) {
            if (m_state.isKeyPressed(GLFW_KEY_A)) {
                UI::UIMain::getPanel<UI::ComponentExplorer>()->toggleVisibility();
            }
        } else {
            if (m_state.isKeyPressed(GLFW_KEY_DELETE)) {
                const auto &sceneState = m_state.getSceneDriver()->getState();
                const auto selectedIds = sceneState.getSelectedComponents() |
                                         std::ranges::views::keys |
                                         std::ranges::to<std::vector<UUID>>();

                std::unordered_set<UUID> moduleCoveredIds;
                std::vector<UUID> moduleIds;
                std::vector<UUID> regularIds;

                std::function<void(const UUID &)> collectCoveredIds = [&](const UUID &uuid) {
                    if (moduleCoveredIds.contains(uuid)) {
                        return;
                    }

                    const auto component = sceneState.getComponentByUuid(uuid);
                    if (!component) {
                        return;
                    }

                    moduleCoveredIds.insert(uuid);
                    for (const auto &dependantUuid : component->getDependants(sceneState)) {
                        collectCoveredIds(dependantUuid);
                    }
                };

                for (const auto &id : selectedIds) {
                    const auto component = sceneState.getComponentByUuid(id);
                    if (!component) {
                        continue;
                    }

                    if (component->getType() == Canvas::SceneComponentType::module) {
                        moduleIds.push_back(id);
                        collectCoveredIds(id);
                    }
                }

                for (const auto &id : selectedIds) {
                    if (!moduleCoveredIds.contains(id)) {
                        regularIds.push_back(id);
                    }
                }

                if (moduleIds.empty() && regularIds.empty()) {
                    return;
                }

                auto deleteCommand = std::make_unique<Cmd::MacroCommand>();
                for (const auto &moduleId : moduleIds) {
                    deleteCommand->addCommand(std::make_unique<Cmd::DeleteModuleCmd>(
                        m_state.getSceneDriver().getActiveScene(),
                        moduleId));
                }

                if (!regularIds.empty()) {
                    deleteCommand->addCommand(std::make_unique<Cmd::DeleteCompCmd>(regularIds));
                }

                m_state.getCommandSystem().execute(std::move(deleteCommand));
            } else if (m_state.isKeyPressed(GLFW_KEY_F)) {
                m_state.getSceneDriver()->focusCameraOnSelected();
            } else if (m_state.isKeyPressed(GLFW_KEY_TAB)) {
                m_state.getSceneDriver()->toggleSchematicView();
            } else if (m_state.isKeyPressed(GLFW_KEY_ESCAPE)) {
                UI::UIMain::getPanel<UI::ComponentExplorer>()->hide();
            } else if (m_state.isKeyPressed(GLFW_KEY_C)) {
                auto &mainPageState = Pages::MainPage::getInstance()->getState();
                auto &sceneDriver = mainPageState.getSceneDriver();
                auto &sceneState = sceneDriver->getState();
                const auto &selComponents = sceneState.getSelectedComponents();
                if (!selComponents.empty()) {
                    sceneDriver.updateNets();
                    std::unordered_set<UUID> processedNetIds;
                    for (const auto &entry : selComponents) {
                        const auto &compId = entry.first;
                        const auto &comp = sceneState.getComponentByUuid(compId);
                        if (!comp ||
                            comp->getType() != Canvas::SceneComponentType::simulation)
                            continue;

                        const auto netId = comp->cast<Canvas::SimulationSceneComponent>()->getNetId();
                        if (netId == UUID::null || processedNetIds.contains(netId)) {
                            continue;
                        }

                        auto module = Canvas::ModuleSceneComponent::fromNet(netId);

                        BESS_ASSERT(module, "Failed to create module");
                        processedNetIds.insert(netId);
                    }
                }
            }
        }
    }

    MainPageState &MainPage::getState() {
        return m_state;
    };

    void MainPage::copySelectedEntities() {
        auto &ctx = Svc::CopyPaste::Context::instance();
        ctx.copy(m_state.getSceneDriver().getActiveScene());
    }

    void MainPage::pasteCopiedEntities() {
        auto &ctx = Svc::CopyPaste::Context::instance();
        ctx.paste(m_state.getSceneDriver().getActiveScene());
    }

} // namespace Bess::Pages
