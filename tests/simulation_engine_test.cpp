#include "common/types.h"
#include "component_catalog.h"
#include "dig_sim_driver.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "plugin_manager.h"
#include "simulation_engine.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>
#include <string_view>
#include <thread>

using namespace std::chrono_literals;

namespace {
    using Bess::UUID;
    using namespace Bess::SimEngine;

    std::shared_ptr<Drivers::CompDef>
    findDefinitionByName(std::string_view name) {
        const auto &components = ComponentCatalog::instance().getComponents();
        const auto it =
            std::ranges::find_if(components, [name](const auto &definition) {
                return definition && definition->getName() == name;
            });

        return it == components.end() ? nullptr : *it;
    }

    void ensurePrimitiveGateDefinitions() {
        auto ensureGate = [](const std::string &name, size_t inputCount,
                             const std::function<LogicState(
                                 const std::vector<SlotState> &)> &eval) {
            if (findDefinitionByName(name)) {
                return;
            }

            auto definition = std::make_shared<Drivers::Digital::DigCompDef>();
            definition->setName(name);
            definition->setGroupName("Logic");
            definition->setInputSlotsInfo(
                {SlotsGroupType::input, false, inputCount, {}, {}});
            definition->setOutputSlotsInfo(
                {SlotsGroupType::output, false, 1, {}, {}});
            definition->setSimFn(
                [eval](const std::shared_ptr<Drivers::Digital::DigCompSimData>
                           &rawData)
                    -> std::shared_ptr<Drivers::Digital::DigCompSimData> {
                    const auto simData = std::dynamic_pointer_cast<
                        Drivers::Digital::DigCompSimData>(rawData);
                    if (!simData) {
                        return rawData;
                    }

                    if (simData->outputStates.empty()) {
                        simData->outputStates.resize(1);
                    }

                    const auto next = eval(simData->inputStates);
                    const auto prev =
                        simData->prevState.outputStates.empty()
                            ? LogicState::unknown
                            : simData->prevState.outputStates[0].state;

                    simData->outputStates[0].state = next;
                    simData->outputStates[0].lastChangeTime =
                        std::chrono::duration_cast<SimTime>(simData->simTime);
                    simData->simDependants = prev != next;
                    return simData;
                });
            definition->setPropDelay(Bess::TimeNs(1));
            ComponentCatalog::instance().registerComponent(definition);
        };

        ensureGate("NOT Gate", 1, [](const std::vector<SlotState> &inputs) {
            const auto inState =
                inputs.empty() ? LogicState::low : inputs[0].state;
            return inState == LogicState::high ? LogicState::low
                                               : LogicState::high;
        });
        ensureGate("AND Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a =
                inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b =
                inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a && b) ? LogicState::high : LogicState::low;
        });
        ensureGate("OR Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a =
                inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b =
                inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a || b) ? LogicState::high : LogicState::low;
        });
        ensureGate("XOR Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a =
                inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b =
                inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a != b) ? LogicState::high : LogicState::low;
        });
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

    PortRef digitalPort(const UUID &uuid, PortDirection direction, int index) {
        return {.componentId = uuid,
                .direction = direction,
                .signalKind = SignalKind::digital,
                .index = index};
    }

    bool slotStateEquals(SimulationEngine &engine, const UUID &uuid,
                         PortDirection direction, int idx,
                         LogicState expected) {
        return engine.getPortState(digitalPort(uuid, direction, idx)).state ==
               expected;
    }

    LogicState boolToState(bool value) {
        return value ? LogicState::high : LogicState::low;
    }
} // namespace

class SimulationEngineTest : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        auto &pluginManager = Bess::Plugins::PluginManager::getInstance();
        // Plugins are loaded globally in main_test.cpp, so we don't need to
        // assert on it here
        pluginManager.loadPluginsFromDirectory("plugins");
    }

    SimulationEngine *engine = nullptr;
    std::shared_ptr<Drivers::CompDef> inputDef, outputDef, notDef, andDef,
        orDef, xorDef;

    void SetUp() override {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        engine = &projectCtx->getSimEngine();
        ensurePrimitiveGateDefinitions();

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
            engine = nullptr;
        }
    }

    UUID addComponent(const std::shared_ptr<Drivers::CompDef> &definition) {
        const auto uuid = engine->addComponent(definition);
        EXPECT_NE(uuid, UUID::null);
        return uuid;
    }

    void driveInput(const UUID &inputId, bool value) {
        engine->setOutputSlotState(inputId, 0, boolToState(value));
    }

    void expectOutputEventually(const UUID &uuid, PortDirection direction, int idx,
                                LogicState expected,
                                std::chrono::milliseconds timeout = 250ms) {
        ASSERT_TRUE(waitUntil(
            [&] {
                return slotStateEquals(*engine, uuid, direction, idx, expected);
            },
            timeout))
            << "Timed out waiting for slot state "
            << static_cast<int>(expected);
    }

    void exerciseBinaryGate(const std::shared_ptr<Drivers::CompDef> &gateDef,
                            const std::array<bool, 4> &expectedOutputs) {
        const auto inputA = addComponent(inputDef);
        const auto inputB = addComponent(inputDef);
        const auto gate = addComponent(gateDef);

        ASSERT_TRUE(engine->connectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));
        ASSERT_TRUE(engine->connectPorts(digitalPort(inputB, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 1)));

        const std::array<std::pair<bool, bool>, 4> rows = {{
            {false, false},
            {false, true},
            {true, false},
            {true, true},
        }};

        for (size_t i = 0; i < rows.size(); ++i) {
            driveInput(inputA, rows[i].first);
            driveInput(inputB, rows[i].second);
            expectOutputEventually(gate, PortDirection::output, 0,
                                   boolToState(expectedOutputs[i]));
        }
    }
};

TEST_F(SimulationEngineTest, CatalogIncludesBuiltInAndPluginDefinitions) {
    EXPECT_NE(findDefinitionByName("Input"), nullptr);
    EXPECT_NE(findDefinitionByName("Output"), nullptr);
    EXPECT_NE(findDefinitionByName("AND Gate"), nullptr);
    EXPECT_NE(findDefinitionByName("OR Gate"), nullptr);
    EXPECT_NE(findDefinitionByName("XOR Gate"), nullptr);
}

TEST_F(SimulationEngineTest, CanConnectComponentsRejectsInvalidConfigurations) {
    const auto input = addComponent(inputDef);
    const auto gate = addComponent(andDef);

    const auto [nullOk, nullError] = engine->canConnectPorts(digitalPort(Bess::UUID::null, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0));
    EXPECT_FALSE(nullOk);
    EXPECT_EQ(nullError, "Cannot connect to/from null component");

    const auto [sameTypeOk, sameTypeError] = engine->canConnectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::output, 0));
    EXPECT_FALSE(sameTypeOk);
    EXPECT_EQ(sameTypeError, "Cannot connect pins of the same type i.e. input "
                             "-> input or output -> output");

    const auto [badIndexOk, badIndexError] = engine->canConnectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 9), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0));
    EXPECT_FALSE(badIndexOk);
    EXPECT_TRUE(badIndexError.starts_with("Invalid source pin index."));
}

TEST_F(SimulationEngineTest,
       ConnectionLifecycleTracksDuplicatesAndComponentDeletion) {
    const auto inputA = addComponent(inputDef);
    const auto inputB = addComponent(inputDef);
    const auto gate = addComponent(andDef);

    const auto [canConnectFirst, firstError] = engine->canConnectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0));
    EXPECT_TRUE(canConnectFirst);
    EXPECT_TRUE(firstError.empty());

    ASSERT_TRUE(engine->connectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));
    ASSERT_TRUE(engine->connectPorts(digitalPort(inputB, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 1)));

    auto [canConnectDuplicate, duplicateError] = engine->canConnectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0));
    EXPECT_FALSE(canConnectDuplicate);
    EXPECT_EQ(duplicateError, "Connection already exists");
    EXPECT_FALSE(engine->connectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));

    auto connections = engine->getConnections(gate);
    ASSERT_EQ(connections.inputs.size(), 2u);
    EXPECT_EQ(connections.inputs[0].size(), 1u);
    EXPECT_EQ(connections.inputs[1].size(), 1u);
    EXPECT_EQ(connections.inputs[0][0].first, inputA);
    EXPECT_EQ(connections.inputs[1][0].first, inputB);

    engine->deleteConnection(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0));

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
    ASSERT_TRUE(engine->connectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));

    driveInput(input, false);
    expectOutputEventually(gate, PortDirection::output, 0, LogicState::high);

    driveInput(input, true);
    expectOutputEventually(gate, PortDirection::output, 0, LogicState::low);
}

TEST_F(SimulationEngineTest, OutputComponentReceivesDrivenSignal) {
    const auto input = addComponent(inputDef);
    const auto sink = addComponent(outputDef);

    ASSERT_TRUE(engine->connectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(sink, Bess::SimEngine::PortDirection::input, 0)));

    driveInput(input, true);
    expectOutputEventually(sink, PortDirection::input, 0, LogicState::high);

    driveInput(input, false);
    expectOutputEventually(sink, PortDirection::input, 0, LogicState::low);
}

TEST_F(SimulationEngineTest, PauseAndStepControlsWhenQueuedSimulationRuns) {
    const auto input = addComponent(inputDef);
    const auto gate = addComponent(notDef);

    ASSERT_TRUE(engine->connectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));

    driveInput(input, false);
    expectOutputEventually(gate, PortDirection::output, 0, LogicState::high);

    engine->setSimulationState(SimulationState::paused);

    driveInput(input, true);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(
        engine->getPortState(digitalPort(gate, Bess::SimEngine::PortDirection::output, 0)).state,
        LogicState::high);

    engine->stepSimulation();
    expectOutputEventually(gate, PortDirection::output, 0, LogicState::low);

    driveInput(input, false);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(
        engine->getPortState(digitalPort(gate, Bess::SimEngine::PortDirection::output, 0)).state,
        LogicState::low);

    engine->stepSimulation();
    expectOutputEventually(gate, PortDirection::output, 0, LogicState::high);
}

TEST_F(SimulationEngineTest, DeleteComponentRemovesItFromStateAndConnections) {
    const auto input = addComponent(inputDef);
    const auto gate = addComponent(notDef);

    ASSERT_TRUE(engine->connectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(gate, Bess::SimEngine::PortDirection::input, 0)));

    engine->deleteComponent(input);

    EXPECT_EQ(engine->getComponent<Drivers::Digital::DigSimComp>(input),
              nullptr);
    const auto connections = engine->getConnections(gate);
    ASSERT_EQ(connections.inputs.size(), 1u);
    EXPECT_TRUE(connections.inputs[0].empty());
}

TEST_F(SimulationEngineTest,
       MultiStageCircuitPropagatesAcrossChainedComponents) {
    const auto inputA = addComponent(inputDef);
    const auto inputB = addComponent(inputDef);
    const auto andGate = addComponent(andDef);
    const auto notGate = addComponent(notDef);
    const auto sink = addComponent(outputDef);

    ASSERT_TRUE(engine->connectPorts(digitalPort(inputA, Bess::SimEngine::PortDirection::output, 0), digitalPort(andGate, Bess::SimEngine::PortDirection::input, 0)));
    ASSERT_TRUE(engine->connectPorts(digitalPort(inputB, Bess::SimEngine::PortDirection::output, 0), digitalPort(andGate, Bess::SimEngine::PortDirection::input, 1)));
    ASSERT_TRUE(engine->connectPorts(digitalPort(andGate, Bess::SimEngine::PortDirection::output, 0), digitalPort(notGate, Bess::SimEngine::PortDirection::input, 0)));
    ASSERT_TRUE(engine->connectPorts(digitalPort(notGate, Bess::SimEngine::PortDirection::output, 0), digitalPort(sink, Bess::SimEngine::PortDirection::input, 0)));

    driveInput(inputA, false);
    driveInput(inputB, false);
    expectOutputEventually(notGate, PortDirection::output, 0,
                           LogicState::high);
    expectOutputEventually(sink, PortDirection::input, 0, LogicState::high);

    driveInput(inputA, true);
    driveInput(inputB, true);
    expectOutputEventually(notGate, PortDirection::output, 0,
                           LogicState::low);
    expectOutputEventually(sink, PortDirection::input, 0, LogicState::low);

    driveInput(inputB, false);
    expectOutputEventually(andGate, PortDirection::output, 0,
                           LogicState::low);
    expectOutputEventually(sink, PortDirection::input, 0, LogicState::high);
}

TEST_F(SimulationEngineTest,
       RepeatedSignalChangesRemainConsistentAcrossSingleChain) {
    const auto input = addComponent(inputDef);
    const auto notGate = addComponent(notDef);
    const auto sink = addComponent(outputDef);
    ASSERT_TRUE(engine->connectPorts(digitalPort(input, Bess::SimEngine::PortDirection::output, 0), digitalPort(notGate, Bess::SimEngine::PortDirection::input, 0)));
    ASSERT_TRUE(engine->connectPorts(digitalPort(notGate, Bess::SimEngine::PortDirection::output, 0), digitalPort(sink, Bess::SimEngine::PortDirection::input, 0)));

    for (const bool value : {false, true, false, true, true, false}) {
        driveInput(input, value);
        expectOutputEventually(notGate, PortDirection::output, 0,
                               boolToState(!value));
        expectOutputEventually(sink, PortDirection::input, 0,
                               boolToState(!value));
    }
}
