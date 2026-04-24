#include "component_catalog.h"
#include "drivers/digital_sim_driver.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "plugin_manager.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "gtest/gtest.h"
#include <memory>
#include <ranges>
#include <string_view>

namespace {
    using namespace Bess::Canvas;
    using namespace Bess::SimEngine;

    std::shared_ptr<Drivers::ComponentDef> findDefinitionByName(std::string_view name) {
        const auto &components = ComponentCatalog::instance().getComponents();
        const auto it = std::ranges::find_if(components, [name](const auto &definition) {
            return definition && definition->getName() == name;
        });

        return it == components.end() ? nullptr : *it;
    }

    struct SimCompFixture {
        std::shared_ptr<SimulationSceneComponent> comp;
        std::vector<std::shared_ptr<SlotSceneComponent>> inputs;
        std::vector<std::shared_ptr<SlotSceneComponent>> outputs;
    };
} // namespace

class ConnectionServiceTest : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
        pluginManager.loadPluginsFromDirectory("plugins");
    }

    void SetUp() override {
        service = &Bess::Svc::SvcConnection::instance();
        service->destroy();
        service->init();

        auto &simEngine = SimulationEngine::instance();
        simEngine.clear();

        inputDef = findDefinitionByName("Input");
        outputDef = findDefinitionByName("Output");

        ASSERT_NE(inputDef, nullptr);
        ASSERT_NE(outputDef, nullptr);

        scene = std::make_unique<Scene>();
    }

    void TearDown() override {
        if (scene) {
            scene->clear();
            scene.reset();
        }

        service->destroy();
        SimulationEngine::instance().clear();
    }

    SimCompFixture addSimComponent(const std::shared_ptr<Drivers::ComponentDef> &definition) {
        auto created = SimulationSceneComponent::createNew(definition);
        SimCompFixture fixture;
        if (created.empty()) {
            ADD_FAILURE() << "No scene components were created";
            return fixture;
        }

        fixture.comp = std::dynamic_pointer_cast<SimulationSceneComponent>(created.front());
        if (!fixture.comp) {
            ADD_FAILURE() << "First created component is not a SimulationSceneComponent";
            return fixture;
        }

        auto &state = scene->getState();
        state.addComponent(fixture.comp);

        for (size_t i = 1; i < created.size(); ++i) {
            auto slot = std::dynamic_pointer_cast<SlotSceneComponent>(created[i]);
            if (!slot) {
                ADD_FAILURE() << "Created child component is not a SlotSceneComponent";
                continue;
            }
            state.addComponent(slot);
            state.attachChild(fixture.comp->getUuid(), slot->getUuid(), false);

            if (slot->isInputSlot()) {
                fixture.inputs.push_back(slot);
            } else {
                fixture.outputs.push_back(slot);
            }
        }

        return fixture;
    }

    Bess::Svc::SvcConnection *service = nullptr;
    std::unique_ptr<Scene> scene;
    std::shared_ptr<Drivers::ComponentDef> inputDef;
    std::shared_ptr<Drivers::ComponentDef> outputDef;
};

TEST_F(ConnectionServiceTest, CanConnectRejectsSameDirectionSlots) {
    const auto left = addSimComponent(inputDef);
    const auto right = addSimComponent(inputDef);

    const auto [canConnect, reason] = service->canConnect(left.outputs.front()->getUuid(),
                                                          right.outputs.front()->getUuid(),
                                                          scene.get());

    EXPECT_FALSE(canConnect);
    EXPECT_EQ(reason, "Cannot connect pins of the same type i.e. input -> input or output -> output");
}

TEST_F(ConnectionServiceTest, CanConnectRejectsDeadSlotIds) {
    const auto left = addSimComponent(inputDef);
    const auto right = addSimComponent(outputDef);

    const auto deadSlotId = right.inputs.front()->getUuid();
    scene->getState().removeComponent(deadSlotId, Bess::UUID::master);

    const auto [canConnect, reason] = service->canConnect(left.outputs.front()->getUuid(),
                                                          deadSlotId,
                                                          scene.get());

    EXPECT_FALSE(canConnect);
    EXPECT_EQ(reason, "Invalid slot components for connection check (Proxy links might be dead)");
}

TEST_F(ConnectionServiceTest, CreateConnectionRegistersConnectionWithSlotsAndScene) {
    const auto source = addSimComponent(inputDef);
    const auto sink = addSimComponent(outputDef);

    auto connection = service->createConnection(source.outputs.front()->getUuid(),
                                                sink.inputs.front()->getUuid(),
                                                scene.get());

    ASSERT_NE(connection, nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()), connection);
    EXPECT_EQ(connection->getStartSlot(), source.outputs.front()->getUuid());
    EXPECT_EQ(connection->getEndSlot(), sink.inputs.front()->getUuid());
    EXPECT_EQ(source.outputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(sink.inputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(source.outputs.front()->getConnectedConnections().front(), connection->getUuid());
    EXPECT_EQ(sink.inputs.front()->getConnectedConnections().front(), connection->getUuid());

    const auto [canConnectAgain, reason] = service->canConnect(source.outputs.front()->getUuid(),
                                                               sink.inputs.front()->getUuid(),
                                                               scene.get());
    EXPECT_FALSE(canConnectAgain);
    EXPECT_EQ(reason, "Connection already exists");
}

TEST_F(ConnectionServiceTest, RemoveConnectionUnregistersConnectionButKeepsNonResizableSlots) {
    const auto source = addSimComponent(inputDef);
    const auto sink = addSimComponent(outputDef);

    auto connection = service->createConnection(source.outputs.front()->getUuid(),
                                                sink.inputs.front()->getUuid(),
                                                scene.get());
    ASSERT_NE(connection, nullptr);

    const auto removedIds = service->removeConnection(connection, scene.get());

    ASSERT_EQ(removedIds.size(), 1u);
    EXPECT_EQ(removedIds.front(), connection->getUuid());
    EXPECT_EQ(scene->getState().getComponentByUuid(connection->getUuid()), nullptr);
    EXPECT_TRUE(source.outputs.front()->getConnectedConnections().empty());
    EXPECT_TRUE(sink.inputs.front()->getConnectedConnections().empty());
    EXPECT_NE(scene->getState().getComponentByUuid(source.outputs.front()->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(sink.inputs.front()->getUuid()), nullptr);
}

TEST_F(ConnectionServiceTest, GetDependantsReturnsEmptyForConnectionWithoutResizableSlots) {
    const auto source = addSimComponent(inputDef);
    const auto sink = addSimComponent(outputDef);

    auto connection = service->createConnection(source.outputs.front()->getUuid(),
                                                sink.inputs.front()->getUuid(),
                                                scene.get());
    ASSERT_NE(connection, nullptr);

    const auto dependants = service->getDependants(connection->getUuid(), scene.get());
    EXPECT_TRUE(dependants.empty());
}

TEST_F(ConnectionServiceTest, SharedSourceCanDriveMultipleIndependentConnections) {
    const auto source = addSimComponent(inputDef);
    const auto leftSink = addSimComponent(outputDef);
    const auto rightSink = addSimComponent(outputDef);

    auto leftConnection = service->createConnection(source.outputs.front()->getUuid(),
                                                    leftSink.inputs.front()->getUuid(),
                                                    scene.get());
    auto rightConnection = service->createConnection(source.outputs.front()->getUuid(),
                                                     rightSink.inputs.front()->getUuid(),
                                                     scene.get());

    ASSERT_NE(leftConnection, nullptr);
    ASSERT_NE(rightConnection, nullptr);
    EXPECT_NE(leftConnection->getUuid(), rightConnection->getUuid());
    EXPECT_EQ(source.outputs.front()->getConnectedConnections().size(), 2u);
    EXPECT_EQ(leftSink.inputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(rightSink.inputs.front()->getConnectedConnections().size(), 1u);

    const auto removedIds = service->removeConnection(leftConnection, scene.get());
    ASSERT_EQ(removedIds.size(), 1u);
    EXPECT_EQ(removedIds.front(), leftConnection->getUuid());
    EXPECT_EQ(source.outputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(source.outputs.front()->getConnectedConnections().front(), rightConnection->getUuid());
    EXPECT_TRUE(leftSink.inputs.front()->getConnectedConnections().empty());
    EXPECT_EQ(rightSink.inputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(rightSink.inputs.front()->getConnectedConnections().front(), rightConnection->getUuid());
}

// ─── Resize Slot Tests ──────────────────────────────────────────────────────

class ResizeSlotConnectionTest : public ConnectionServiceTest {
  protected:
    void SetUp() override {
        ConnectionServiceTest::SetUp();

        // Create an AND-gate-like definition with resizable inputs directly,
        // rather than relying on the Python plugin catalog.
        auto def = std::make_shared<Drivers::Digital::DigCompDef>();
        def->setName("AND Gate (Test)");
        def->setGroupName("Logic");
        def->setInputSlotsInfo({SlotsGroupType::input, true, 2, {"A", "B"}, {}});
        def->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"Y"}, {}});
        def->setOpInfo({'*', false});
        def->setPropDelay(Bess::TimeNs(1));
        def->setSimFn([](const std::shared_ptr<Drivers::SimFnDataBase> &dataBase) {
            return dataBase;
        });
        andDef = def;
    }

    std::shared_ptr<Drivers::ComponentDef> andDef;
};

TEST_F(ResizeSlotConnectionTest, AddSlotIncreasesSimEngineInputCountAndDefMetadata) {
    const auto gate = addSimComponent(andDef);
    ASSERT_NE(gate.comp, nullptr);
    auto &simEngine = SimulationEngine::instance();

    const auto simId = gate.comp->getSimEngineId();
    ASSERT_NE(simId, Bess::UUID::null);

    const auto digComp = simEngine.getDigitalComponent(simId);
    ASSERT_NE(digComp, nullptr);

    const auto digDef = digComp->getDefinition<Drivers::Digital::DigCompDef>();
    ASSERT_NE(digDef, nullptr);

    const size_t originalInputCount = digDef->getInputSlotsInfo().count;
    const size_t originalStateSize = digComp->getInputStates().size();
    const size_t originalConnSize = digComp->getInputConnections().size();

    bool added = simEngine.addSlot(simId, Bess::SimEngine::SlotType::digitalInput,
                                   static_cast<int>(originalInputCount));

    EXPECT_TRUE(added);
    EXPECT_EQ(digDef->getInputSlotsInfo().count, originalInputCount + 1);
    EXPECT_EQ(digComp->getInputStates().size(), originalStateSize + 1);
    EXPECT_EQ(digComp->getInputConnections().size(), originalConnSize + 1);
}

TEST_F(ResizeSlotConnectionTest, RemoveSlotDecreasesSimEngineInputCountAndDefMetadata) {
    const auto gate = addSimComponent(andDef);
    auto &simEngine = SimulationEngine::instance();

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    ASSERT_NE(digComp, nullptr);
    const auto digDef = digComp->getDefinition<Drivers::Digital::DigCompDef>();
    ASSERT_NE(digDef, nullptr);

    // First grow the gate to 3 inputs
    simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 2);
    ASSERT_EQ(digDef->getInputSlotsInfo().count, 3u);

    bool removed = simEngine.removeSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 2);

    EXPECT_TRUE(removed);
    EXPECT_EQ(digDef->getInputSlotsInfo().count, 2u);
    EXPECT_EQ(digComp->getInputStates().size(), 2u);
    EXPECT_EQ(digComp->getInputConnections().size(), 2u);
}

TEST_F(ResizeSlotConnectionTest, RemoveSlotOnNonResizeableOutputReturnsFalse) {
    const auto gate = addSimComponent(andDef);
    auto &simEngine = SimulationEngine::instance();

    // AND gate output is NOT resizeable
    bool removed = simEngine.removeSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalOutput, 0);
    EXPECT_FALSE(removed);
}

TEST_F(ResizeSlotConnectionTest, AddSlotOnNonResizeableOutputReturnsFalse) {
    const auto gate = addSimComponent(andDef);
    auto &simEngine = SimulationEngine::instance();

    bool added = simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalOutput, 0);
    EXPECT_FALSE(added);
}

TEST_F(ResizeSlotConnectionTest, ConnectToNewlyAddedSlotSucceeds) {
    const auto gate = addSimComponent(andDef);
    const auto source = addSimComponent(inputDef);
    auto &simEngine = SimulationEngine::instance();

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    ASSERT_NE(digComp, nullptr);

    // Add a third input slot
    bool added = simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 2);
    ASSERT_TRUE(added);
    ASSERT_EQ(digComp->getInputConnections().size(), 3u);

    // Connect source output 0 → gate input 2
    bool connected = simEngine.connectComponent(
        source.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
        gate.comp->getSimEngineId(), 2, Bess::SimEngine::SlotType::digitalInput);

    EXPECT_TRUE(connected);
    EXPECT_FALSE(digComp->getInputConnections()[2].empty());
}

TEST_F(ResizeSlotConnectionTest, MultipleAddSlotsThenConnectEachSlot) {
    const auto gate = addSimComponent(andDef);
    auto &simEngine = SimulationEngine::instance();

    // Grow from 2 to 5 inputs
    for (int i = 2; i < 5; ++i) {
        EXPECT_TRUE(simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, i));
    }

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    ASSERT_EQ(digComp->getInputConnections().size(), 5u);
    ASSERT_EQ(digComp->getInputStates().size(), 5u);

    // Connect an Input component to each of the 5 slots
    for (int i = 0; i < 5; ++i) {
        const auto src = addSimComponent(inputDef);
        bool ok = simEngine.connectComponent(
            src.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
            gate.comp->getSimEngineId(), i, Bess::SimEngine::SlotType::digitalInput);
        EXPECT_TRUE(ok) << "Connection to input slot " << i << " failed";
    }

    // Verify all 5 input connections are populated
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(digComp->getInputConnections()[i].empty())
            << "Input connection " << i << " is empty after connect";
    }
}

TEST_F(ResizeSlotConnectionTest, RemoveSlotWithExistingConnectionClearsIt) {
    const auto gate = addSimComponent(andDef);
    const auto source = addSimComponent(inputDef);
    auto &simEngine = SimulationEngine::instance();

    // Add slot at index 2, connect to it, then remove it
    ASSERT_TRUE(simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 2));
    ASSERT_TRUE(simEngine.connectComponent(
        source.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
        gate.comp->getSimEngineId(), 2, Bess::SimEngine::SlotType::digitalInput));

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    ASSERT_EQ(digComp->getInputConnections().size(), 3u);

    // Remove last slot (index 2)
    EXPECT_TRUE(simEngine.removeSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 2));
    EXPECT_EQ(digComp->getInputConnections().size(), 2u);
    EXPECT_EQ(digComp->getInputStates().size(), 2u);
}

TEST_F(ResizeSlotConnectionTest, AddSlotForUnknownComponentReturnsFalse) {
    auto &simEngine = SimulationEngine::instance();
    bool added = simEngine.addSlot(Bess::UUID(99999), Bess::SimEngine::SlotType::digitalInput, 0);
    EXPECT_FALSE(added);
}

TEST_F(ResizeSlotConnectionTest, RemoveSlotForUnknownComponentReturnsFalse) {
    auto &simEngine = SimulationEngine::instance();
    bool removed = simEngine.removeSlot(Bess::UUID(99999), Bess::SimEngine::SlotType::digitalInput, 0);
    EXPECT_FALSE(removed);
}

TEST_F(ResizeSlotConnectionTest, CanConnectRejectsOutOfBoundsSlotIndex) {
    const auto gate = addSimComponent(andDef);
    const auto source = addSimComponent(inputDef);
    auto &simEngine = SimulationEngine::instance();

    // AND gate has 2 inputs (indices 0, 1). Trying index 2 should fail.
    auto [ok, msg] = simEngine.canConnectComponents(
        source.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
        gate.comp->getSimEngineId(), 2, Bess::SimEngine::SlotType::digitalInput);

    EXPECT_FALSE(ok);
    EXPECT_NE(msg.find("Invalid"), std::string::npos);
}

TEST_F(ResizeSlotConnectionTest, CanConnectRejectsNegativeSlotIndex) {
    const auto gate = addSimComponent(andDef);
    const auto source = addSimComponent(inputDef);
    auto &simEngine = SimulationEngine::instance();

    auto [ok, msg] = simEngine.canConnectComponents(
        source.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
        gate.comp->getSimEngineId(), -1, Bess::SimEngine::SlotType::digitalInput);

    EXPECT_FALSE(ok);
}

TEST_F(ResizeSlotConnectionTest, DeleteConnectionViaDriverUpdatesIsConnectedFlags) {
    const auto gate = addSimComponent(andDef);
    const auto source = addSimComponent(inputDef);
    auto &simEngine = SimulationEngine::instance();

    simEngine.connectComponent(
        source.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalOutput,
        gate.comp->getSimEngineId(), 0, Bess::SimEngine::SlotType::digitalInput);

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    EXPECT_TRUE(digComp->getIsInputConnected()[0]);

    simEngine.deleteConnection(
        source.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalOutput, 0,
        gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 0);

    EXPECT_FALSE(digComp->getIsInputConnected()[0]);
}

TEST_F(ResizeSlotConnectionTest, DefinitionSlotCountStaysInSyncAfterMultipleResizes) {
    const auto gate = addSimComponent(andDef);
    auto &simEngine = SimulationEngine::instance();

    const auto digComp = simEngine.getDigitalComponent(gate.comp->getSimEngineId());
    const auto digDef = digComp->getDefinition<Drivers::Digital::DigCompDef>();

    // Add 3 more slots (2 → 5)
    for (int i = 2; i < 5; ++i) {
        simEngine.addSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, i);
    }
    EXPECT_EQ(digDef->getInputSlotsInfo().count, 5u);
    EXPECT_EQ(digComp->getInputStates().size(), 5u);

    // Remove 2 slots (5 → 3)
    simEngine.removeSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 4);
    simEngine.removeSlot(gate.comp->getSimEngineId(), Bess::SimEngine::SlotType::digitalInput, 3);
    EXPECT_EQ(digDef->getInputSlotsInfo().count, 3u);
    EXPECT_EQ(digComp->getInputStates().size(), 3u);
    EXPECT_EQ(digComp->getInputConnections().size(), 3u);
    EXPECT_EQ(digComp->getIsInputConnected().size(), 3u);
}
