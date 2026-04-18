#include "component_catalog.h"
#include "component_definition.h"
#include "pages/main_page/scene_components/conn_joint_scene_component.h"
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

    std::shared_ptr<ComponentDefinition> findDefinitionByName(std::string_view name) {
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

    SimCompFixture addSimComponent(const std::shared_ptr<ComponentDefinition> &definition) {
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
    std::shared_ptr<ComponentDefinition> inputDef;
    std::shared_ptr<ComponentDefinition> outputDef;
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

TEST_F(ConnectionServiceTest, CreateConnectionSupportsJointEndpoint) {
    const auto source = addSimComponent(inputDef);
    const auto sink = addSimComponent(outputDef);
    const auto branchSource = addSimComponent(inputDef);

    auto baseConnection = service->createConnection(source.outputs.front()->getUuid(),
                                                    sink.inputs.front()->getUuid(),
                                                    scene.get());
    ASSERT_NE(baseConnection, nullptr);

    auto joint = std::make_shared<ConnJointSceneComp>(baseConnection->getUuid(), 0, ConnSegOrientaion::horizontal);
    joint->setOutputSlotId(source.outputs.front()->getUuid());
    joint->setInputSlotId(sink.inputs.front()->getUuid());
    scene->getState().addComponent(joint);

    const auto [canConnect, reason] = service->canConnect(branchSource.outputs.front()->getUuid(),
                                                          joint->getUuid(),
                                                          scene.get());
    ASSERT_TRUE(canConnect) << reason;

    auto branchConnection = service->createConnection(branchSource.outputs.front()->getUuid(),
                                                      joint->getUuid(),
                                                      scene.get());
    ASSERT_NE(branchConnection, nullptr);

    const auto sceneConnection = scene->getState().getComponentByUuid(branchConnection->getUuid());
    ASSERT_NE(sceneConnection, nullptr);
    EXPECT_EQ(branchConnection->getStartSlot(), branchSource.outputs.front()->getUuid());
    EXPECT_EQ(branchConnection->getEndSlot(), joint->getUuid());
    EXPECT_EQ(branchSource.outputs.front()->getConnectedConnections().size(), 1u);
    EXPECT_EQ(branchSource.outputs.front()->getConnectedConnections().front(), branchConnection->getUuid());
    EXPECT_EQ(joint->getConnections().size(), 1u);
    EXPECT_EQ(joint->getConnections().front(), branchConnection->getUuid());
}

TEST_F(ConnectionServiceTest, AnalogGroundedSourceResistorCircuitSolvesThroughSceneConnections) {
    const auto sourceDef = findDefinitionByName("DC Voltage Source");
    const auto resistorDef = findDefinitionByName("Resistor");
    const auto groundDef = findDefinitionByName("Ground");
    ASSERT_NE(sourceDef, nullptr);
    ASSERT_NE(resistorDef, nullptr);
    ASSERT_NE(groundDef, nullptr);

    const auto source = addSimComponent(sourceDef);
    const auto resistor = addSimComponent(resistorDef);
    const auto ground = addSimComponent(groundDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(resistor.comp, nullptr);
    ASSERT_NE(ground.comp, nullptr);
    ASSERT_EQ(source.comp->getInputSlots().size(), 1u);
    ASSERT_EQ(source.comp->getOutputSlots().size(), 1u);
    ASSERT_EQ(resistor.comp->getInputSlots().size(), 1u);
    ASSERT_EQ(resistor.comp->getOutputSlots().size(), 1u);
    ASSERT_EQ(ground.comp->getInputSlots().size(), 1u);

    const auto sourcePlus = source.comp->getInputSlots()[0];
    const auto sourceMinus = source.comp->getOutputSlots()[0];
    const auto resistorPlus = resistor.comp->getInputSlots()[0];
    const auto resistorMinus = resistor.comp->getOutputSlots()[0];
    const auto groundPin = ground.comp->getInputSlots()[0];

    ASSERT_NE(service->createConnection(sourceMinus, groundPin, scene.get()), nullptr);
    ASSERT_NE(service->createConnection(sourcePlus, resistorPlus, scene.get()), nullptr);
    ASSERT_NE(service->createConnection(sourceMinus, resistorMinus, scene.get()), nullptr);

    auto &engine = SimulationEngine::instance();
    const auto solution = engine.solveAnalogCircuit();

    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto sourceState = engine.getAnalogComponentState(source.comp->getSimEngineId());
    ASSERT_EQ(sourceState.terminals.size(), 2u);
    EXPECT_NEAR(sourceState.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(sourceState.terminals[1].voltage, 0.0, 1e-9);

    const auto resistorState = engine.getAnalogComponentState(resistor.comp->getSimEngineId());
    ASSERT_EQ(resistorState.terminals.size(), 2u);
    EXPECT_NEAR(resistorState.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(resistorState.terminals[1].voltage, 0.0, 1e-9);
    EXPECT_NEAR(resistorState.terminals[0].current, 0.005, 1e-12);
    EXPECT_NEAR(resistorState.terminals[1].current, -0.005, 1e-12);
}

TEST_F(ConnectionServiceTest, AnalogFloatingSourceResistorCircuitAutoReferencesThroughSceneConnections) {
    const auto sourceDef = findDefinitionByName("DC Voltage Source");
    const auto resistorDef = findDefinitionByName("Resistor");
    ASSERT_NE(sourceDef, nullptr);
    ASSERT_NE(resistorDef, nullptr);

    const auto source = addSimComponent(sourceDef);
    const auto resistor = addSimComponent(resistorDef);

    ASSERT_NE(source.comp, nullptr);
    ASSERT_NE(resistor.comp, nullptr);
    ASSERT_EQ(source.comp->getInputSlots().size(), 1u);
    ASSERT_EQ(source.comp->getOutputSlots().size(), 1u);
    ASSERT_EQ(resistor.comp->getInputSlots().size(), 1u);
    ASSERT_EQ(resistor.comp->getOutputSlots().size(), 1u);

    const auto sourcePlus = source.comp->getInputSlots()[0];
    const auto sourceMinus = source.comp->getOutputSlots()[0];
    const auto resistorPlus = resistor.comp->getInputSlots()[0];
    const auto resistorMinus = resistor.comp->getOutputSlots()[0];

    ASSERT_NE(service->createConnection(sourcePlus, resistorPlus, scene.get()), nullptr);
    ASSERT_NE(service->createConnection(sourceMinus, resistorMinus, scene.get()), nullptr);

    auto &engine = SimulationEngine::instance();
    const auto solution = engine.solveAnalogCircuit();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NE(solution.message.find("auto-referenced"), std::string::npos);
}
