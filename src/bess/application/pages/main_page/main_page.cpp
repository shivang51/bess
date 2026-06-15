#include "pages/main_page/main_page.h"
#include "bess_core/asset_manager/asset_manager.h"
#include "bess_core/connection_service.h"
#include "bess_core/copy_paste_service.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "macro_command.h"
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
#include "plugin_manager.h"
#include "scene/widgets/scene_widgets.h"
#include "scene_ser_reg.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/input_sub_system_types.h"
#include "ui/ui.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/ui_main/ui_main.h"
#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <ranges>
#include <unordered_set>

namespace Bess::Pages {
    bool MainPage::s_headless = false;

    void MainPage::setHeadless(bool headless) {
        s_headless = headless;
    }

    std::shared_ptr<MainPage> &
    MainPage::getInstance(const std::shared_ptr<Window> &parentWindow) {
        static auto instance = std::make_shared<MainPage>(parentWindow);
        return instance;
    }

    MainPage::MainPage(const std::shared_ptr<Window> &parentWindow) {
        if (!s_headless && m_parentWindow == nullptr &&
            parentWindow == nullptr) {
            throw std::runtime_error("MainPage: parentWindow is nullptr. Need "
                                     "to pass a parent window.");
        }
        m_parentWindow = parentWindow;

        // TODO(shivang): Think about a better way and scalabilty for plugins
        Canvas::NonSimSceneComponent::registerComponent<Canvas::TextComponent>(
            "Text Component");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::WidgetsTestComponent>("Widgets Test");
        Canvas::NonSimSceneComponent::registerComponent<
            Canvas::SlotProbeSceneComponent>("Probe");

        REG_TO_SER_REGISTRY(Canvas::ConnJointSceneComp);
        REG_TO_SER_REGISTRY(Canvas::ConnectionSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::GroupSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::InputSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::NonSimSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SimulationSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::TextComponent);
        REG_TO_SER_REGISTRY(Canvas::WidgetsTestComponent);
        REG_TO_SER_REGISTRY(Canvas::SlotProbeSceneComponent);
        REG_TO_SER_REGISTRY(Canvas::ModuleSceneComponent);

        if (!s_headless) {
            UI::UIMain::init();
        }

        m_state.init();

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

        Canvas::NonSimSceneComponent::clearRegistry();
        Canvas::SceneSerReg::clearRegistry();

        auto &appCtx = Bess::GAppContext::getInstance();
        if (!s_headless) {
            UI::UIMain::destroy();
        }

        m_state.getSceneDriver()->reset();
        appCtx.getSubSystem<Assets::AssetManager>()->clear();

        BESS_INFO("[MainPage] Destroyed");
        m_isDestroyed = true;
    }

    void MainPage::draw() {
        UI::UIMain::draw();

        const auto plugins =
            Plugins::PluginManager::getInstance().getLoadedPlugins();
        for (const auto &plugin : plugins) {
            if (!plugin.second)
                continue;
            plugin.second->drawUI();
        }
    }

    void MainPage::update(TimeMs ts) {
        m_state.update();

        const auto activeScene = m_state.getSceneDriver()->getActiveScene();
        const bool sceneWantsKeyboard =
            activeScene &&
            Canvas::SceneWidgets::wantsKeyboard(&activeScene->getState());
        const bool imguiWantsKeyboard =
            ImGui::GetIO().WantTextInput || sceneWantsKeyboard;

        if (!imguiWantsKeyboard)
            handleKeyboardShortcuts();

        // dispatching events after handling keyboard shortcuts,
        // so all modification are synced before updaing UI
        auto &appCtx = GAppContext::getInstance();
        auto eventDispatcher =
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();
        eventDispatcher->dispatchAll();

        UI::UIMain::update(ts);
    }

    std::shared_ptr<Window> MainPage::getParentWindow() {
        return m_parentWindow;
    }

    void MainPage::handleKeyboardShortcuts() {

        auto &appCtx = GAppContext::getInstance();
        auto inpSystem = appCtx.getSubSystem<Bess::InputSubSystem>();

        const bool isCtrlPressed = inpSystem->isCtrlPressed();
        const bool isShiftPressed = inpSystem->isShiftPressed();

        if (isCtrlPressed) {
            if (inpSystem->isKeyPressed(KeyCode::s)) {
                m_state.actionFlags.saveProject = true;
            } else if (inpSystem->isKeyPressed(KeyCode::o)) {
                m_state.actionFlags.openProject = true;
            } else if (inpSystem->isKeyPressed(KeyCode::z)) {
                if (isShiftPressed) {
                    m_state.getCommandSystem().redo();
                } else {
                    m_state.getCommandSystem().undo();
                }
            } else if (inpSystem->isKeyPressed(KeyCode::g)) {
                UI::UIMain::getPanel<UI::ProjectExplorer>()
                    ->groupSelectedNodes();
            } else if (inpSystem->isKeyPressed(KeyCode::a)) {
                auto targetScene = UI::UIMain::getTargetViewportScene();
                if (!targetScene) {
                    targetScene = m_state.getSceneDriver()->getActiveScene();
                }
                if (targetScene) {
                    targetScene->selectAllEntities();
                }
            } else if (inpSystem->isKeyPressed(KeyCode::c)) {
                copySelectedEntities();
            } else if (inpSystem->isKeyPressed(KeyCode::v)) {
                pasteCopiedEntities();
            }
        } else if (isShiftPressed) {
            if (inpSystem->isKeyPressed(KeyCode::a)) {
                UI::UIMain::getPanel<UI::ComponentExplorer>()
                    ->toggleVisibility();
            }
        } else {
            if (inpSystem->isKeyPressed(KeyCode::del)) {
                auto targetScene = UI::UIMain::getTargetViewportScene();
                if (!targetScene) {
                    targetScene = m_state.getSceneDriver()->getActiveScene();
                }
                if (!targetScene) {
                    return;
                }

                const auto &sceneState = targetScene->getState();
                const auto selectedIds = sceneState.getSelectedComponents() |
                                         std::ranges::views::keys |
                                         std::ranges::to<std::vector<UUID>>();

                std::unordered_set<UUID> moduleCoveredIds;
                std::vector<UUID> moduleIds;
                std::vector<UUID> regularIds;

                std::function<void(const UUID &)> collectCoveredIds =
                    [&](const UUID &uuid) {
                        if (moduleCoveredIds.contains(uuid)) {
                            return;
                        }

                        const auto component =
                            sceneState.getComponentByUuid(uuid);
                        if (!component) {
                            return;
                        }

                        moduleCoveredIds.insert(uuid);
                        for (const auto &dependantUuid :
                             component->getDependants(sceneState)) {
                            collectCoveredIds(dependantUuid);
                        }
                    };

                for (const auto &id : selectedIds) {
                    const auto component = sceneState.getComponentByUuid(id);
                    if (!component) {
                        continue;
                    }

                    if (component->getType() ==
                        Canvas::SceneComponentType::module) {
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
                    deleteCommand->addCommand(
                        std::make_unique<Cmd::DeleteModuleCmd>(targetScene,
                                                               moduleId));
                }

                if (!regularIds.empty()) {
                    deleteCommand->addCommand(
                        std::make_unique<Cmd::DeleteCompCmd>(regularIds));
                }

                deleteCommand->setSceneContext(targetScene);
                m_state.getCommandSystem().execute(std::move(deleteCommand));
            } else if (inpSystem->isKeyPressed(KeyCode::f)) {
                if (const auto targetPanel =
                        UI::UIMain::getTargetSceneViewportPanel()) {
                    targetPanel->focusCameraOnSelected();
                }
            } else if (inpSystem->isKeyPressed(KeyCode::tab)) {
                auto targetScene = UI::UIMain::getTargetViewportScene();
                if (!targetScene) {
                    targetScene = m_state.getSceneDriver()->getActiveScene();
                }
                if (targetScene) {
                    targetScene->toggleSchematicView();
                }
            } else if (inpSystem->isKeyPressed(KeyCode::escape)) {
                UI::UIMain::getPanel<UI::ComponentExplorer>()->hide();
            } else if (inpSystem->isKeyPressed(KeyCode::c)) {
                auto &mainPageState =
                    Pages::MainPage::getInstance()->getState();
                auto sceneDriver = mainPageState.getSceneDriver();
                auto &sceneState = sceneDriver->getActiveScene()->getState();
                const auto selectedIds = sceneState.getSelectedComponents() |
                                         std::views::keys |
                                         std::ranges::to<std::vector<UUID>>();
                if (!selectedIds.empty()) {
                    m_state.updateNets(sceneDriver->getActiveScene());
                    std::unordered_set<UUID> processedNetIds;
                    std::vector<UUID> netIdsToModule;
                    netIdsToModule.reserve(selectedIds.size());

                    for (const auto &compId : selectedIds) {
                        const auto &comp =
                            sceneState.getComponentByUuid(compId);
                        if (!comp || comp->getType() !=
                                         Canvas::SceneComponentType::simulation)
                            continue;

                        const auto netId =
                            comp->cast<Canvas::SimulationSceneComponent>()
                                ->getNetId();
                        if (netId == UUID::null ||
                            processedNetIds.contains(netId)) {
                            continue;
                        }

                        netIdsToModule.push_back(netId);
                        processedNetIds.insert(netId);
                    }

                    for (const auto &netId : netIdsToModule) {
                        auto module =
                            Canvas::ModuleSceneComponent::fromNet(netId);
                        BESS_ASSERT(module, "Failed to create module");
                    }
                }
            }
        }
    }

    MainPageState &MainPage::getState() {
        return m_state;
    };

    void MainPage::copySelectedEntities() {
        auto projCtx =
            GAppContext::getInstance().getSubSystem<Bess::ProjectContext>();
        auto ctx = projCtx->getSubSystem<Svc::CopyPaste::Context>();
        auto targetScene = UI::UIMain::getTargetViewportScene();
        if (!targetScene) {
            targetScene = m_state.getSceneDriver()->getActiveScene();
        }
        if (targetScene) {
            ctx->copy(targetScene);
        }
    }

    void MainPage::pasteCopiedEntities() {
        auto projCtx =
            GAppContext::getInstance().getSubSystem<Bess::ProjectContext>();
        auto ctx = projCtx->getSubSystem<Svc::CopyPaste::Context>();
        auto targetScene = UI::UIMain::getTargetViewportScene();
        if (!targetScene) {
            targetScene = m_state.getSceneDriver()->getActiveScene();
        }
        if (targetScene) {
            ctx->paste(targetScene, {0.f, 0.f});
        }
    }

} // namespace Bess::Pages
