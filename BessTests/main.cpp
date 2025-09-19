#include "bess_uuid.h"
#include "simulation_engine.h"
#include <gtest/gtest.h>

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

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
