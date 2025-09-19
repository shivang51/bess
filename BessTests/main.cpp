#include "bess_uuid.h"
#include "simulation_engine.h"
#include <gtest/gtest.h>
#include <thread>

class SimulationEngineTest : public testing::Test {
  protected:
    SimulationEngineTest() {}

    ~SimulationEngineTest() override {}

    Bess::SimEngine::SimulationEngine simEngine_;
};

TEST_F(SimulationEngineTest, MinorConfig) {
    ASSERT_EQ(simEngine_.getSimulationState(), Bess::SimEngine::SimulationState::running);
    simEngine_.toggleSimState();
    ASSERT_EQ(simEngine_.getSimulationState(), Bess::SimEngine::SimulationState::paused);
    simEngine_.toggleSimState();
    ASSERT_EQ(simEngine_.getSimulationState(), Bess::SimEngine::SimulationState::running);
}

TEST_F(SimulationEngineTest, AddingRemovingComponents) {
    auto uuid = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);
    EXPECT_NE(uuid, Bess::UUID::null);
    uuid = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 3);
    EXPECT_NE(uuid, Bess::UUID::null);
    uuid = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 4);
    EXPECT_NE(uuid, Bess::UUID::null);
}

TEST_F(SimulationEngineTest, ComponentConnection) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto and_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);

    bool success = simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, and_gate, 0, Bess::SimEngine::PinType::input);
    ASSERT_TRUE(success);

    success = simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, and_gate, 1, Bess::SimEngine::PinType::input);
    ASSERT_TRUE(success);

    auto connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[0].size(), 1);
    ASSERT_EQ(connections.inputs[1].size(), 1);
    ASSERT_EQ(connections.outputs[0].size(), 0);

    ASSERT_EQ(connections.inputs[0][0].first, input1);
    ASSERT_EQ(connections.inputs[1][0].first, input2);

    simEngine_.deleteConnection(input1, Bess::SimEngine::PinType::output, 0, and_gate, Bess::SimEngine::PinType::input, 0);
    connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[0].size(), 0);
    ASSERT_EQ(connections.inputs[1].size(), 1);

    simEngine_.deleteConnection(input2, Bess::SimEngine::PinType::output, 0, and_gate, Bess::SimEngine::PinType::input, 1);
    connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[0].size(), 0);
    ASSERT_EQ(connections.inputs[1].size(), 0);
}

TEST_F(SimulationEngineTest, AndGateLogic) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto and_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);

    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, and_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, and_gate, 1, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(input1, false);
    simEngine_.setDigitalInput(input2, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(and_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, true);
    simEngine_.setDigitalInput(input2, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(and_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, false);
    simEngine_.setDigitalInput(input2, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(and_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, true);
    simEngine_.setDigitalInput(input2, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(and_gate, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, OrGateLogic) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto or_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::OR, 2);

    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, or_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, or_gate, 1, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(input1, false);
    simEngine_.setDigitalInput(input2, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(or_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, true);
    simEngine_.setDigitalInput(input2, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(or_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, false);
    simEngine_.setDigitalInput(input2, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(or_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, true);
    simEngine_.setDigitalInput(input2, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(or_gate, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, NotGateLogic) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto not_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::NOT, 1);

    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, not_gate, 0, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(input1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(input1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, DeleteComponent) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto and_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);

    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, and_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, and_gate, 1, Bess::SimEngine::PinType::input);

    simEngine_.deleteComponent(input1);
    auto connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[0].size(), 0);
    ASSERT_EQ(connections.inputs[1].size(), 1);

    simEngine_.deleteComponent(and_gate);
    connections = simEngine_.getConnections(input2);
    ASSERT_EQ(connections.outputs[0].size(), 0);
}

TEST_F(SimulationEngineTest, PauseAndStep) {
    auto input = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto not_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::NOT);

    simEngine_.connectComponent(input, 0, Bess::SimEngine::PinType::output, not_gate, 0, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(input, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.toggleSimState(); // Pause
    simEngine_.setDigitalInput(input, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0));

    simEngine_.stepSimulation();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, Clock) {
    auto clock = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    simEngine_.updateClock(clock, true, 1, Bess::SimEngine::FrequencyUnit::hz);
    bool last_state = simEngine_.getDigitalPinState(clock, Bess::SimEngine::PinType::output, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    bool current_state = simEngine_.getDigitalPinState(clock, Bess::SimEngine::PinType::output, 0);
    ASSERT_NE(last_state, current_state);

    simEngine_.updateClock(clock, false, 1, Bess::SimEngine::FrequencyUnit::hz);
    last_state = simEngine_.getDigitalPinState(clock, Bess::SimEngine::PinType::output, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    current_state = simEngine_.getDigitalPinState(clock, Bess::SimEngine::PinType::output, 0);
    ASSERT_EQ(last_state, current_state);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
