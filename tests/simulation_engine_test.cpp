#include "component_catalog.h"
#include "component_definition.h"
#include "gtest/gtest.h"
#include "plugin_manager.h"
#include "simulation_engine.h"
#include "types.h"
#include <chrono>
#include <memory>
#include <ranges>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace {
    using Bess::UUID;
    using namespace Bess::SimEngine;

    std::shared_ptr<ComponentDefinition> findDefinitionByName(std::string_view name) {
        const auto &components = ComponentCatalog::instance().getComponents();
        const auto it = std::ranges::find_if(components, [name](const auto &definition) {
            return definition && definition->getName() == name;
        });

        return it == components.end() ? nullptr : *it;
    }

    bool waitUntil(const std::function<bool()> &predicate,
                   std::chrono::milliseconds timeout = 250ms,
                   std::chrono::milliseconds poll = 2ms) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) {
                return true;
            }
            std::this_thread::sleep_for(poll);
        }
        return predicate();
    }

    bool slotStateEquals(SimulationEngine &engine, const UUID &uuid, SlotType type, int idx, LogicState expected) {
        return engine.getDigitalSlotState(uuid, type, idx).state == expected;
    }

    LogicState boolToState(bool value) {
        return value ? LogicState::high : LogicState::low;
    }
} // namespace

class SimulationEngineTest : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
        ASSERT_TRUE(pluginManager.loadPluginsFromDirectory("plugins"));
    }

    void SetUp() override {
        engine = std::make_unique<SimulationEngine>();

        inputDef = findDefinitionByName("Input");
        outputDef = findDefinitionByName("Output");
        notDef = findDefinitionByName("NOT Gate");
        andDef = findDefinitionByName("AND Gate");
        orDef = findDefinitionByName("OR Gate");
        xorDef = findDefinitionByName("XOR Gate");

        ASSERT_NE(inputDef, nullptr);
        ASSERT_NE(outputDef, nullptr);
        ASSERT_NE(notDef, nullptr);
        ASSERT_NE(andDef, nullptr);
        ASSERT_NE(orDef, nullptr);
        ASSERT_NE(xorDef, nullptr);

        engine->setSimulationState(SimulationState::running);
        engine->clear();
    }

    void TearDown() override {
        if (engine) {
            engine->setSimulationState(SimulationState::paused);
            std::this_thread::sleep_for(10ms);
            engine->clear();
            engine->destroy();
            engine.reset();
        }
    }

    UUID addComponent(const std::shared_ptr<ComponentDefinition> &definition) {
        const auto uuid = engine->addComponent(definition);
        EXPECT_NE(uuid, UUID::null);
        return uuid;
    }

    void driveInput(const UUID &inputId, bool value) {
        engine->setOutputSlotState(inputId, 0, boolToState(value));
    }

    void expectOutputEventually(const UUID &uuid,
                                SlotType type,
                                int idx,
                                LogicState expected,
                                std::chrono::milliseconds timeout = 250ms) {
        ASSERT_TRUE(waitUntil([&] {
            return slotStateEquals(*engine, uuid, type, idx, expected);
        }, timeout))
            << "Timed out waiting for slot state " << static_cast<int>(expected);
    }

    void exerciseBinaryGate(const std::shared_ptr<ComponentDefinition> &gateDef,
                            const std::array<bool, 4> &expectedOutputs) {
        const auto inputA = addComponent(inputDef);
        const auto inputB = addComponent(inputDef);
        const auto gate = addComponent(gateDef);

        ASSERT_TRUE(engine->connectComponent(inputA, 0, SlotType::digitalOutput,
                                             gate, 0, SlotType::digitalInput));
        ASSERT_TRUE(engine->connectComponent(inputB, 0, SlotType::digitalOutput,
                                             gate, 1, SlotType::digitalInput));

        const std::array<std::pair<bool, bool>, 4> rows = {{
            {false, false},
            {false, true},
            {true, false},
            {true, true},
        }};

        for (size_t i = 0; i < rows.size(); ++i) {
            driveInput(inputA, rows[i].first);
            driveInput(inputB, rows[i].second);
            expectOutputEventually(gate, SlotType::digitalOutput, 0, boolToState(expectedOutputs[i]));
        }
    }

    std::unique_ptr<SimulationEngine> engine;
    std::shared_ptr<ComponentDefinition> inputDef;
    std::shared_ptr<ComponentDefinition> outputDef;
    std::shared_ptr<ComponentDefinition> notDef;
    std::shared_ptr<ComponentDefinition> andDef;
    std::shared_ptr<ComponentDefinition> orDef;
    std::shared_ptr<ComponentDefinition> xorDef;
};

TEST_F(SimulationEngineTest, CatalogIncludesBuiltInAndPluginDefinitions) {
    EXPECT_NE(findDefinitionByName("Input"), nullptr);
    EXPECT_NE(findDefinitionByName("Output"), nullptr);
    EXPECT_NE(findDefinitionByName("Clock"), nullptr);
    EXPECT_NE(findDefinitionByName("AND Gate"), nullptr);
    EXPECT_NE(findDefinitionByName("OR Gate"), nullptr);
    EXPECT_NE(findDefinitionByName("XOR Gate"), nullptr);
}

TEST_F(SimulationEngineTest, ConnectionLifecycleTracksDuplicatesAndComponentDeletion) {
    const auto inputA = addComponent(inputDef);
    const auto inputB = addComponent(inputDef);
    const auto gate = addComponent(andDef);

    const auto [canConnectFirst, firstError] = engine->canConnectComponents(
        inputA, 0, SlotType::digitalOutput, gate, 0, SlotType::digitalInput);
    EXPECT_TRUE(canConnectFirst);
    EXPECT_TRUE(firstError.empty());

    ASSERT_TRUE(engine->connectComponent(inputA, 0, SlotType::digitalOutput,
                                         gate, 0, SlotType::digitalInput));
    ASSERT_TRUE(engine->connectComponent(inputB, 0, SlotType::digitalOutput,
                                         gate, 1, SlotType::digitalInput));

    auto [canConnectDuplicate, duplicateError] = engine->canConnectComponents(
        inputA, 0, SlotType::digitalOutput, gate, 0, SlotType::digitalInput);
    EXPECT_FALSE(canConnectDuplicate);
    EXPECT_EQ(duplicateError, "Connection already exists");
    EXPECT_FALSE(engine->connectComponent(inputA, 0, SlotType::digitalOutput,
                                          gate, 0, SlotType::digitalInput));

    auto connections = engine->getConnections(gate);
    ASSERT_EQ(connections.inputs.size(), 2u);
    EXPECT_EQ(connections.inputs[0].size(), 1u);
    EXPECT_EQ(connections.inputs[1].size(), 1u);
    EXPECT_EQ(connections.inputs[0][0].first, inputA);
    EXPECT_EQ(connections.inputs[1][0].first, inputB);

    engine->deleteConnection(inputA, SlotType::digitalOutput, 0,
                             gate, SlotType::digitalInput, 0);

    connections = engine->getConnections(gate);
    EXPECT_TRUE(connections.inputs[0].empty());
    EXPECT_EQ(connections.inputs[1].size(), 1u);

    engine->deleteComponent(inputB);
    connections = engine->getConnections(gate);
    EXPECT_TRUE(connections.inputs[1].empty());

    std::this_thread::sleep_for(20ms);
}

TEST_F(SimulationEngineTest, BasicGateTruthTablesProduceExpectedOutputs) {
    exerciseBinaryGate(andDef, {false, false, false, true});
    exerciseBinaryGate(orDef, {false, true, true, true});
    exerciseBinaryGate(xorDef, {false, true, true, false});

    const auto input = addComponent(inputDef);
    const auto gate = addComponent(notDef);
    ASSERT_TRUE(engine->connectComponent(input, 0, SlotType::digitalOutput,
                                         gate, 0, SlotType::digitalInput));

    driveInput(input, false);
    expectOutputEventually(gate, SlotType::digitalOutput, 0, LogicState::high);

    driveInput(input, true);
    expectOutputEventually(gate, SlotType::digitalOutput, 0, LogicState::low);
}

TEST_F(SimulationEngineTest, OutputComponentReceivesDrivenSignal) {
    const auto input = addComponent(inputDef);
    const auto sink = addComponent(outputDef);

    ASSERT_TRUE(engine->connectComponent(input, 0, SlotType::digitalOutput,
                                         sink, 0, SlotType::digitalInput));

    driveInput(input, true);
    expectOutputEventually(sink, SlotType::digitalInput, 0, LogicState::high);

    driveInput(input, false);
    expectOutputEventually(sink, SlotType::digitalInput, 0, LogicState::low);
}

TEST_F(SimulationEngineTest, PauseAndStepControlsWhenQueuedSimulationRuns) {
    const auto input = addComponent(inputDef);
    const auto gate = addComponent(notDef);

    ASSERT_TRUE(engine->connectComponent(input, 0, SlotType::digitalOutput,
                                         gate, 0, SlotType::digitalInput));

    driveInput(input, false);
    expectOutputEventually(gate, SlotType::digitalOutput, 0, LogicState::high);

    engine->setSimulationState(SimulationState::paused);

    driveInput(input, true);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(engine->getDigitalSlotState(gate, SlotType::digitalOutput, 0).state, LogicState::high);

    engine->stepSimulation();
    expectOutputEventually(gate, SlotType::digitalOutput, 0, LogicState::low);

    driveInput(input, false);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(engine->getDigitalSlotState(gate, SlotType::digitalOutput, 0).state, LogicState::low);

    engine->stepSimulation();
    expectOutputEventually(gate, SlotType::digitalOutput, 0, LogicState::high);
}
