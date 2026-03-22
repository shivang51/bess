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
#include <memory>
#include <ranges>

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
                auto ids = m_state.getSceneDriver()->getState().getSelectedComponents() |
                           std::ranges::views::keys |
                           std::ranges::to<std::vector<UUID>>();

                m_state.getCommandSystem().execute(std::make_unique<Cmd::DeleteCompCmd>(ids));
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
                    for (const auto &entry : selComponents) {
                        const auto &compId = entry.first;
                        const auto &comp = sceneState.getComponentByUuid(compId);
                        if (!comp ||
                            comp->getType() != Canvas::SceneComponentType::simulation)
                            continue;

                        auto module = Canvas::ModuleSceneComponent::fromNet(
                            comp->cast<Canvas::SimulationSceneComponent>()->getNetId());

                        BESS_ASSERT(module, "Failed to create module");
                        sceneDriver->addComponent(module);
                    }
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

        auto &ctx = Svc::CopyPaste::Context::instance();
        ctx.clear();

        for (const auto &selId : selComponents) {
            const auto comp = sceneState.getComponentByUuid(selId);
            const auto type = comp->getType();

            Svc::CopyPaste::CopiedEntity entity{};
            entity.type = type;

            switch (type) {
            case Canvas::SceneComponentType::simulation: {
                const auto casted = comp->cast<Canvas::SimulationSceneComponent>();
                entity.data = Svc::CopyPaste::ETSimComp{casted};
            } break;
            case Canvas::SceneComponentType::nonSimulation: {
                const auto casted = comp->cast<Canvas::NonSimSceneComponent>();
                entity.data = Svc::CopyPaste::ETNonSimComp{casted->getTypeIndex(), casted};
            } break;
            case Canvas::SceneComponentType::connection: {
                const auto casted = comp->cast<Canvas::ConnectionSceneComponent>();
                entity.data = Svc::CopyPaste::ETConnection{casted};
            } break;
            default:
                continue;
                break;
            }

            entity.pos = comp->getTransform().position;

            ctx.addEntity(entity);
        }
    }

    void MainPage::pasteCopiedEntities() {
        auto &cmdSystem = m_state.getCommandSystem();
        auto &scene = m_state.getSceneDriver();

        auto &ctx = Svc::CopyPaste::Context::instance();
        ctx.calcCenter();

        auto macroCmd = std::make_unique<Cmd::MacroCommand>();

        const auto &newCenter = scene->getSceneMousePos();
        std::vector<Svc::CopyPaste::CopiedEntity> connEntites;

        std::unordered_map<UUID, UUID> ogToClonedIdMap;

        for (auto &entity : ctx.getEntites()) {
            const auto pos = newCenter + entity.pos - ctx.getCenter();
            if (entity.type == Canvas::SceneComponentType::simulation) {
                const auto &entityData = std::get<Svc::CopyPaste::ETSimComp>(entity.data);
                auto clonedComponents = entityData.comp->clone(scene->getState());
                BESS_ASSERT(!clonedComponents.empty(),
                            "Simulation clone returned no components");

                auto clonedComp = std::dynamic_pointer_cast<Canvas::SimulationSceneComponent>(
                    clonedComponents.front());
                BESS_ASSERT(clonedComp,
                            "Simulation clone did not return a simulation component first");
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() == entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");

                clonedComp->getTransform().position.x = pos.x;
                clonedComp->getTransform().position.y = pos.y;

                ogToClonedIdMap[entityData.comp->getUuid()] = clonedComp->getUuid();

                size_t idx = 0;
                for (const auto &ogId : entityData.comp->getInputSlots()) {
                    const auto &clonedId = clonedComp->getInputSlots()[idx];
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                idx = 0;
                for (const auto &ogId : entityData.comp->getOutputSlots()) {
                    const auto &clonedId = clonedComp->getOutputSlots()[idx];
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::SimulationSceneComponent>>(
                    clonedComp,
                    clonedComponents);
                macroCmd->addCommand(std::move(addCmd));
            } else if (entity.type == Canvas::SceneComponentType::nonSimulation) {
                const auto &entityData = std::get<Svc::CopyPaste::ETNonSimComp>(entity.data);
                auto clonedComponents = entityData.comp->clone(scene->getState());
                BESS_ASSERT(!clonedComponents.empty(), "Non-simulation clone returned no components");

                auto inst = std::dynamic_pointer_cast<Canvas::NonSimSceneComponent>(clonedComponents.front());
                BESS_ASSERT(inst, "Non-simulation clone did not return a non-simulation component first");
                clonedComponents.erase(clonedComponents.begin());
                BESS_ASSERT(clonedComponents.size() == entityData.comp->getChildComponents().size(),
                            "[Paste] Not all child comps got cloned");
                inst->getTransform().position.x = pos.x;
                inst->getTransform().position.y = pos.y;

                ogToClonedIdMap[entityData.comp->getUuid()] = inst->getUuid();

                size_t idx = 0;
                for (const auto &ogId : entityData.comp->getChildComponents()) {
                    const auto &clonedId = clonedComponents[idx]->getUuid();
                    ogToClonedIdMap[ogId] = clonedId;
                    idx++;
                }

                auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::NonSimSceneComponent>>(inst, clonedComponents);
                macroCmd->addCommand(std::move(addCmd));
            } else if (entity.type == Canvas::SceneComponentType::connection) {
                connEntites.push_back(entity);
            }
        }

        for (const auto &connEntity : connEntites) {
            const auto &entityData = std::get<Svc::CopyPaste::ETConnection>(connEntity.data);
            const auto &ogConn = entityData.conn->cast<Canvas::ConnectionSceneComponent>();
            auto clonedComps = ogConn->cloneConn(scene->getState(),
                                                 ogToClonedIdMap);
            auto clonedConn = clonedComps.front()->cast<Canvas::ConnectionSceneComponent>();
            clonedComps.erase(clonedComps.begin());

            auto addCmd = std::make_unique<Cmd::AddCompCmd<Canvas::ConnectionSceneComponent>>(clonedConn, clonedComps);
            macroCmd->addCommand(std::move(addCmd));
        }

        cmdSystem.execute(std::move(macroCmd));
    }

} // namespace Bess::Pages
