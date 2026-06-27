#include "bess_core/commands/add_component_command.h"
#include "bess_core/commands/command_system.h"
#include "bess_core/commands/delete_component_command.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "dig_sim_driver.h"
#include "event_dispatcher.h"
#include "pages/main_page/main_page_command_hooks.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "simulation_engine.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace {
    using Bess::UUID;
    using Bess::Canvas::ConnectionSceneComponent;
    using Bess::Canvas::Scene;
    using Bess::Canvas::SceneComponent;
    using Bess::Canvas::SimulationSceneComponent;
    using Bess::Canvas::SlotSceneComponent;
    using Bess::SimEngine::Drivers::Digital::DigCompDef;

    std::shared_ptr<DigCompDef> makeDefinition(std::string name,
                                               size_t inputCount,
                                               size_t outputCount,
                                               bool inputsResizable = false,
                                               bool outputsResizable = false,
                                               bool keepIOCountEq = false,
                                               char op = '0') {
        auto definition = std::make_shared<DigCompDef>();
        definition->setName(name);
        definition->setGroupName("Test");
        definition->setKeepIOCountEq(keepIOCountEq);
        Bess::SimEngine::OperatorInfo opInfo;
        opInfo.op = op;
        definition->setOpInfo(opInfo);
        definition->setInputSlotsInfo({Bess::SimEngine::SlotsGroupType::input,
                                       inputsResizable,
                                       inputCount,
                                       {},
                                       {}});
        definition->setOutputSlotsInfo({Bess::SimEngine::SlotsGroupType::output,
                                        outputsResizable,
                                        outputCount,
                                        {},
                                        {}});
        definition->setPropDelay(Bess::TimeNs(1));
        definition->setSimFn(
            [](const std::shared_ptr<
                Bess::SimEngine::Drivers::Digital::DigCompSimData> &data) {
                return data;
            });
        return definition;
    }

    bool containsUuid(const std::vector<UUID> &values, const UUID &id) {
        return std::ranges::find(values, id) != values.end();
    }

    struct SimComponentFixture {
        std::shared_ptr<SimulationSceneComponent> comp;
        std::vector<std::shared_ptr<SlotSceneComponent>> inputs;
        std::vector<std::shared_ptr<SlotSceneComponent>> outputs;
        std::vector<std::shared_ptr<SceneComponent>> children;
    };

    SimComponentFixture createSimComponent(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &definition) {
        const auto created = SimulationSceneComponent::createNew(definition);
        SimComponentFixture fixture;
        if (created.empty()) {
            return fixture;
        }

        fixture.comp =
            std::dynamic_pointer_cast<SimulationSceneComponent>(created[0]);
        for (size_t i = 1; i < created.size(); ++i) {
            fixture.children.push_back(created[i]);
            auto slot =
                std::dynamic_pointer_cast<SlotSceneComponent>(created[i]);
            if (!slot) {
                continue;
            }

            if (slot->isInputSlot()) {
                fixture.inputs.push_back(slot);
            } else {
                fixture.outputs.push_back(slot);
            }
        }
        return fixture;
    }
} // namespace

class MainPageConnectionCommandsTest : public testing::Test {
  protected:
    void SetUp() override {
        auto &appCtx = Bess::GAppContext::getInstance();
        if (!appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.addSubSystem<Bess::EventSystem::EventDispatcher>()->onInit();
        } else {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()
                ->dispatchAll();
        }

        if (!appCtx.hasSubSystem<Bess::ProjectContext>()) {
            projectContext = appCtx.addSubSystem<Bess::ProjectContext>();
            projectContext->addSubSystem<Bess::Svc::SvcConnection>();
            projectContext->onInit();
        } else {
            projectContext = appCtx.getSubSystem<Bess::ProjectContext>();
            projectContext->addSubSystem<Bess::Svc::SvcConnection>();
            if (!projectContext->hasSubSystem<Bess::SceneDriver>()) {
                projectContext->onInit();
            }
        }

        sceneDriver = projectContext->getSubSystem<Bess::SceneDriver>();
        simEngine =
            projectContext->getSubSystem<Bess::SimEngine::SimulationEngine>();
        connectionService =
            projectContext->getSubSystem<Bess::Svc::SvcConnection>();
        commandSystem =
            projectContext->getSubSystem<Bess::Cmd::CommandSystem>();

        connectionService->onDestroy();
        connectionService->onInit();

        simEngine->clear();
        simEngine->setSimulationState(Bess::SimEngine::SimulationState::paused);

        sceneDriver->removeScenes();
        scene = std::make_shared<Scene>(false);
        scene->getState().setIsRootScene(true);
        sceneDriver->addScene(scene);
        sceneDriver->setRootSceneId(scene->getSceneId());
        sceneDriver->setActiveScene(scene->getSceneId());

        commandSystem->reset();
        commandSystem->setScene(scene);
        commandSystem->setSceneComponentHooks(
            Bess::Pages::createMainPageCommandHooks());

        sourceDef = makeDefinition("Source", 0, 1);
        sinkDef = makeDefinition("Sink", 1, 0);
    }

    void TearDown() override {
        if (commandSystem) {
            commandSystem->reset();
            commandSystem->setScene(nullptr);
        }

        if (scene) {
            scene->clear();
            scene.reset();
        }

        if (sceneDriver) {
            sceneDriver->removeScenes();
        }

        if (simEngine) {
            simEngine->clear();
            simEngine->setSimulationState(
                Bess::SimEngine::SimulationState::paused);
        }

        if (connectionService) {
            connectionService->onDestroy();
            connectionService->onInit();
        }

        auto &appCtx = Bess::GAppContext::getInstance();
        if (projectContext) {
            projectContext->onDestroy();
            projectContext.reset();
        }
        if (appCtx.hasSubSystem<Bess::ProjectContext>()) {
            appCtx.removeSubSystem<Bess::ProjectContext>();
        }

        if (appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
        }
    }

    SimComponentFixture addSimComponent(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &definition) {
        auto fixture = createSimComponent(definition);
        EXPECT_NE(fixture.comp, nullptr);
        if (!fixture.comp) {
            return fixture;
        }

        commandSystem->execute(
            std::make_unique<Bess::Cmd::AddCompCmd<SimulationSceneComponent>>(
                fixture.comp, fixture.children));
        EXPECT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()),
                  nullptr);
        EXPECT_NE(fixture.comp->getSimEngineId(), UUID::null);
        return fixture;
    }

    void expectConnectionRestored(
        const SimComponentFixture &source,
        const SimComponentFixture &sink,
        const std::shared_ptr<ConnectionSceneComponent> &connection) const {
        ASSERT_NE(connection, nullptr);
        ASSERT_FALSE(source.outputs.empty());
        ASSERT_FALSE(sink.inputs.empty());

        EXPECT_NE(scene->getState().getComponentByUuid(source.comp->getUuid()),
                  nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(
                      source.outputs.front()->getUuid()),
                  nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
                  nullptr);
        EXPECT_TRUE(
            containsUuid(source.outputs.front()->getConnectedConnections(),
                         connection->getUuid()));
        EXPECT_TRUE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                                 connection->getUuid()));
        EXPECT_NE(source.comp->getSimEngineId(), UUID::null);
    }

    size_t simSlotCount(const SimComponentFixture &fixture,
                        bool inputSlots) const {
        const auto digComp =
            simEngine
                ->getComponent<Bess::SimEngine::Drivers::Digital::DigSimComp>(
                    fixture.comp->getSimEngineId());
        EXPECT_NE(digComp, nullptr);
        if (!digComp) {
            return 0;
        }

        const auto digDef = digComp->getDefinition<DigCompDef>();
        EXPECT_NE(digDef, nullptr);
        if (!digDef) {
            return 0;
        }

        return inputSlots ? digDef->getInputSlotsInfo().count
                          : digDef->getOutputSlotsInfo().count;
    }

    size_t sceneRealSlotCount(const SimComponentFixture &fixture,
                              bool inputSlots) const {
        const auto &slotIds = inputSlots ? fixture.comp->getInputSlots()
                                         : fixture.comp->getOutputSlots();
        size_t count = 0;
        for (const auto &slotId : slotIds) {
            const auto slot =
                scene->getState().getComponentByUuid<SlotSceneComponent>(
                    slotId);
            if (slot && !slot->isResizeSlot()) {
                ++count;
            }
        }
        return count;
    }

    std::shared_ptr<Bess::ProjectContext> projectContext;
    std::shared_ptr<Bess::SceneDriver> sceneDriver;
    std::shared_ptr<Bess::SimEngine::SimulationEngine> simEngine;
    std::shared_ptr<Bess::Svc::SvcConnection> connectionService;
    std::shared_ptr<Bess::Cmd::CommandSystem> commandSystem;
    std::shared_ptr<Scene> scene;
    std::shared_ptr<DigCompDef> sourceDef;
    std::shared_ptr<DigCompDef> sinkDef;
};

TEST(SimulationSceneComponentSlotDirtyTest, SlotMutatorsMarkUIDirty) {
    auto comp = std::make_shared<SimulationSceneComponent>();

    const UUID inputA;
    comp->setUIDirty(false);
    comp->addInputSlot(inputA, false);
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getInputSlots(), inputA));

    const UUID inputB;
    comp->setUIDirty(false);
    comp->insertInputSlot(inputB, 0);
    EXPECT_TRUE(comp->getUIDirty());
    ASSERT_FALSE(comp->getInputSlots().empty());
    EXPECT_EQ(comp->getInputSlots().front(), inputB);

    comp->setUIDirty(false);
    EXPECT_TRUE(comp->removeInputSlot(inputA));
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_FALSE(containsUuid(comp->getInputSlots(), inputA));

    comp->setUIDirty(false);
    EXPECT_FALSE(comp->removeInputSlot(inputA));
    EXPECT_FALSE(comp->getUIDirty());

    const UUID outputA;
    comp->setUIDirty(false);
    comp->addOutputSlot(outputA);
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getOutputSlots(), outputA));

    const std::vector<UUID> outputSlots{outputA};
    comp->setUIDirty(false);
    comp->setOutputSlots(outputSlots);
    EXPECT_FALSE(comp->getUIDirty());

    const UUID outputB;
    comp->setUIDirty(false);
    comp->setOutputSlots({outputA, outputB});
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_TRUE(containsUuid(comp->getOutputSlots(), outputB));

    comp->setUIDirty(false);
    EXPECT_TRUE(comp->removeOutputSlot(outputA));
    EXPECT_TRUE(comp->getUIDirty());
    EXPECT_FALSE(containsUuid(comp->getOutputSlots(), outputA));
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteComponentUndoRestoresItsConnections) {
    const auto source = addSimComponent(sourceDef);
    const auto sink = addSimComponent(sinkDef);
    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(sink.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());
    ASSERT_FALSE(sink.inputs.empty());

    auto connection =
        connectionService->createConnection(source.outputs.front()->getUuid(),
                                            sink.inputs.front()->getUuid(),
                                            scene);
    ASSERT_NE(connection, nullptr);
    expectConnectionRestored(source, sink, connection);

    commandSystem->execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<UUID>{source.comp->getUuid()}));

    EXPECT_TRUE(commandSystem->canUndo());
    EXPECT_EQ(scene->getState().getComponentByUuid(source.comp->getUuid()),
              nullptr);
    EXPECT_EQ(
        scene->getState().getComponentByUuid(source.outputs.front()->getUuid()),
        nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    commandSystem->undo();
    expectConnectionRestored(source, sink, connection);

    ASSERT_TRUE(commandSystem->canRedo());
    commandSystem->redo();
    EXPECT_EQ(scene->getState().getComponentByUuid(source.comp->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    ASSERT_TRUE(commandSystem->canUndo());
    commandSystem->undo();
    expectConnectionRestored(source, sink, connection);
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteConnectionUndoRestoresRemovedResizableSlotInSimEngine) {
    const auto resizableSourceDef =
        makeDefinition("Resizable Source", 0, 2, false, true);
    const auto source = addSimComponent(resizableSourceDef);
    const auto sink = addSimComponent(sinkDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(sink.comp, nullptr);
    ASSERT_GE(source.outputs.size(), 3u);
    ASSERT_FALSE(sink.inputs.empty());

    auto sourceSlot = source.outputs[1];
    ASSERT_FALSE(sourceSlot->isResizeSlot());
    ASSERT_EQ(sourceSlot->getIndex(), 1);
    ASSERT_EQ(simSlotCount(source, false), 2u);

    auto connection = connectionService->createConnection(
        sourceSlot->getUuid(), sink.inputs.front()->getUuid(), scene);
    ASSERT_NE(connection, nullptr);
    EXPECT_TRUE(containsUuid(sourceSlot->getConnectedConnections(),
                             connection->getUuid()));

    commandSystem->execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<UUID>{connection->getUuid()}));

    EXPECT_TRUE(commandSystem->canUndo());
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(sourceSlot->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(source, false), 1u);
    EXPECT_FALSE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                              connection->getUuid()));

    commandSystem->undo();

    EXPECT_NE(scene->getState().getComponentByUuid(sourceSlot->getUuid()),
              nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(source, false), 2u);
    EXPECT_EQ(sourceSlot->getIndex(), 1);
    EXPECT_TRUE(containsUuid(sourceSlot->getConnectedConnections(),
                             connection->getUuid()));
    EXPECT_TRUE(containsUuid(sink.inputs.front()->getConnectedConnections(),
                             connection->getUuid()));
}

TEST_F(MainPageConnectionCommandsTest,
       DeleteConnectionUndoDoesNotDuplicateNotGatePairedOutputSlot) {
    Bess::Pages::MainPageState mainPageState;
    mainPageState.init();

    const auto notDef =
        makeDefinition("NOT Gate", 1, 1, true, false, true, '!');
    const auto source = addSimComponent(sourceDef);
    const auto notGate = addSimComponent(notDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(notGate.comp, nullptr);
    ASSERT_FALSE(source.outputs.empty());
    ASSERT_GE(notGate.inputs.size(), 2u);

    auto dispatcher = Bess::GAppContext::getInstance()
                          .getSubSystem<Bess::EventSystem::EventDispatcher>();
    dispatcher->dispatchAll();

    const auto resizeInput = notGate.inputs.back();
    ASSERT_TRUE(resizeInput->isResizeSlot());

    auto connection = connectionService->createConnection(
        source.outputs.front()->getUuid(), resizeInput->getUuid(), scene);
    ASSERT_NE(connection, nullptr);
    dispatcher->dispatchAll();

    const auto restoredInputId = connection->getEndSlot();
    ASSERT_NE(restoredInputId, resizeInput->getUuid());
    const auto restoredInput =
        scene->getState().getComponentByUuid<SlotSceneComponent>(
            restoredInputId);
    ASSERT_NE(restoredInput, nullptr);
    ASSERT_EQ(restoredInput->getIndex(), 1);
    ASSERT_EQ(simSlotCount(notGate, true), 2u);
    ASSERT_EQ(simSlotCount(notGate, false), 2u);
    ASSERT_EQ(sceneRealSlotCount(notGate, true), 2u);
    ASSERT_EQ(sceneRealSlotCount(notGate, false), 2u);
    ASSERT_GE(notGate.comp->getOutputSlots().size(), 2u);

    const auto pairedOutputId = notGate.comp->getOutputSlots()[1];
    const auto outputCountBeforeDelete = notGate.comp->getOutputSlots().size();
    ASSERT_NE(scene->getState().getComponentByUuid<SlotSceneComponent>(
                  pairedOutputId),
              nullptr);

    commandSystem->execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<UUID>{connection->getUuid()}));
    dispatcher->dispatchAll();

    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(restoredInputId), nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(pairedOutputId), nullptr);
    EXPECT_EQ(simSlotCount(notGate, true), 1u);
    EXPECT_EQ(simSlotCount(notGate, false), 1u);
    EXPECT_EQ(sceneRealSlotCount(notGate, true), 1u);
    EXPECT_EQ(sceneRealSlotCount(notGate, false), 1u);

    ASSERT_TRUE(commandSystem->canUndo());
    commandSystem->undo();
    dispatcher->dispatchAll();

    EXPECT_NE(scene->getState().getComponentByUuid(restoredInputId), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(pairedOutputId), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(connection->getUuid()),
              nullptr);
    EXPECT_EQ(simSlotCount(notGate, true), 2u);
    EXPECT_EQ(simSlotCount(notGate, false), 2u);
    EXPECT_EQ(sceneRealSlotCount(notGate, true), 2u);
    EXPECT_EQ(sceneRealSlotCount(notGate, false), 2u);
    EXPECT_EQ(notGate.comp->getOutputSlots().size(), outputCountBeforeDelete);
    EXPECT_TRUE(containsUuid(notGate.comp->getOutputSlots(), pairedOutputId));

    const auto outputIds = notGate.comp->getOutputSlots();
    const std::unordered_set<UUID> uniqueOutputIds(outputIds.begin(),
                                                   outputIds.end());
    EXPECT_EQ(uniqueOutputIds.size(), outputIds.size());
}
