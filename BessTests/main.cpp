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

TEST_F(SimulationEngineTest, ComponentManipulation) {
    // Add components
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto and_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);

    EXPECT_NE(input1, Bess::UUID::null);
    EXPECT_NE(input2, Bess::UUID::null);
    EXPECT_NE(and_gate, Bess::UUID::null);

    // Connect components
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

    // Delete connection
    simEngine_.deleteConnection(input1, Bess::SimEngine::PinType::output, 0, and_gate, Bess::SimEngine::PinType::input, 0);
    connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[0].size(), 0);
    ASSERT_EQ(connections.inputs[1].size(), 1);

    // Delete component
    simEngine_.deleteComponent(input2);
    connections = simEngine_.getConnections(and_gate);
    ASSERT_EQ(connections.inputs[1].size(), 0);
}


TEST_F(SimulationEngineTest, GateLogic) {
    auto input1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto input2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);

    auto and_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::AND, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, and_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, and_gate, 1, Bess::SimEngine::PinType::input);

    auto or_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::OR, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, or_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, or_gate, 1, Bess::SimEngine::PinType::input);

    auto not_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::NOT, 1);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, not_gate, 0, Bess::SimEngine::PinType::input);

    auto nand_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::NAND, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, nand_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, nand_gate, 1, Bess::SimEngine::PinType::input);

    auto nor_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::NOR, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, nor_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, nor_gate, 1, Bess::SimEngine::PinType::input);

    auto xor_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::XOR, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, xor_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, xor_gate, 1, Bess::SimEngine::PinType::input);

    auto xnor_gate = simEngine_.addComponent(Bess::SimEngine::ComponentType::XNOR, 2);
    simEngine_.connectComponent(input1, 0, Bess::SimEngine::PinType::output, xnor_gate, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(input2, 0, Bess::SimEngine::PinType::output, xnor_gate, 1, Bess::SimEngine::PinType::input);

    // Truth Table
    bool i1[] = {false, true, false, true};
    bool i2[] = {false, false, true, true};

    bool and_expected[] = {false, false, false, true};
    bool or_expected[] = {false, true, true, true};
    bool not_expected[] = {true, false, true, false};
    bool nand_expected[] = {true, true, true, false};
    bool nor_expected[] = {true, false, false, false};
    bool xor_expected[] = {false, true, true, false};
    bool xnor_expected[] = {true, false, false, true};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(input1, i1[i]);
        simEngine_.setDigitalInput(input2, i2[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(and_gate, Bess::SimEngine::PinType::output, 0), and_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(or_gate, Bess::SimEngine::PinType::output, 0), or_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(not_gate, Bess::SimEngine::PinType::output, 0), not_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(nand_gate, Bess::SimEngine::PinType::output, 0), nand_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(nor_gate, Bess::SimEngine::PinType::output, 0), nor_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(xor_gate, Bess::SimEngine::PinType::output, 0), xor_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(xnor_gate, Bess::SimEngine::PinType::output, 0), xnor_expected[i]);
    }
}

TEST_F(SimulationEngineTest, SRLatch) {
    // SR Latch using NAND gates
    auto S = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto R = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);

    auto nand1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::NAND, 2);
    auto nand2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::NAND, 2);

    // Q output from nand1, Q' output from nand2
    simEngine_.connectComponent(S, 0, Bess::SimEngine::PinType::output, nand1, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(nand2, 0, Bess::SimEngine::PinType::output, nand1, 1, Bess::SimEngine::PinType::input);

    simEngine_.connectComponent(R, 0, Bess::SimEngine::PinType::output, nand2, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(nand1, 0, Bess::SimEngine::PinType::output, nand2, 1, Bess::SimEngine::PinType::input);

    // Initial state (S=1, R=1) -> latch
    simEngine_.setDigitalInput(S, true);
    simEngine_.setDigitalInput(R, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    bool q = simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0);
    bool q_prime = simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0);
    ASSERT_EQ(q, !q_prime);

    // Set (S=0, R=1) -> Q=1
    simEngine_.setDigitalInput(S, false);
    simEngine_.setDigitalInput(R, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0));
    ASSERT_FALSE(simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0));

    // Latch (S=1, R=1) -> Q=1
    simEngine_.setDigitalInput(S, true);
    simEngine_.setDigitalInput(R, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0));
    ASSERT_FALSE(simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0));

    // Reset (S=1, R=0) -> Q=0
    simEngine_.setDigitalInput(S, true);
    simEngine_.setDigitalInput(R, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0));

    // Latch (S=1, R=1) -> Q=0
    simEngine_.setDigitalInput(S, true);
    simEngine_.setDigitalInput(R, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0));

    // Invalid state (S=0, R=0) -> Q=1, Q'=1
    simEngine_.setDigitalInput(S, false);
    simEngine_.setDigitalInput(R, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand1, Bess::SimEngine::PinType::output, 0));
    ASSERT_TRUE(simEngine_.getDigitalPinState(nand2, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, FullAdderLogic) {
    auto A = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto B = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto Cin = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);

    auto full_adder = simEngine_.addComponent(Bess::SimEngine::ComponentType::FULL_ADDER);
    simEngine_.connectComponent(A, 0, Bess::SimEngine::PinType::output, full_adder, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(B, 0, Bess::SimEngine::PinType::output, full_adder, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(Cin, 0, Bess::SimEngine::PinType::output, full_adder, 2, Bess::SimEngine::PinType::input);

    bool iA[] = {false, false, false, false, true, true, true, true};
    bool iB[] = {false, false, true, true, false, false, true, true};
    bool iCin[] = {false, true, false, true, false, true, false, true};

    bool sum_expected[] = {false, true, true, false, true, false, false, true};
    bool cout_expected[] = {false, false, false, true, false, true, true, true};

    for (int i = 0; i < 8; i++) {
        simEngine_.setDigitalInput(A, iA[i]);
        simEngine_.setDigitalInput(B, iB[i]);
        simEngine_.setDigitalInput(Cin, iCin[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ASSERT_EQ(simEngine_.getDigitalPinState(full_adder, Bess::SimEngine::PinType::output, 0), sum_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(full_adder, Bess::SimEngine::PinType::output, 1), cout_expected[i]);
    }
}

TEST_F(SimulationEngineTest, HalfAdderLogic) {
    auto a = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto b = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto half_adder = simEngine_.addComponent(Bess::SimEngine::ComponentType::HALF_ADDER);

    simEngine_.connectComponent(a, 0, Bess::SimEngine::PinType::output, half_adder, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(b, 0, Bess::SimEngine::PinType::output, half_adder, 1, Bess::SimEngine::PinType::input);

    bool i1[] = {false, true, false, true};
    bool i2[] = {false, false, true, true};

    bool sum_expected[] = {false, true, true, false};
    bool carry_expected[] = {false, false, false, true};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(a, i1[i]);
        simEngine_.setDigitalInput(b, i2[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(half_adder, Bess::SimEngine::PinType::output, 0), sum_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(half_adder, Bess::SimEngine::PinType::output, 1), carry_expected[i]);
    }
}

TEST_F(SimulationEngineTest, MultiplexerLogic) {
    // 2x1 MUX
    auto i0_2x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i1_2x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto sel_2x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto mux_2x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::MULTIPLEXER_2_1);

    simEngine_.connectComponent(i0_2x1, 0, Bess::SimEngine::PinType::output, mux_2x1, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i1_2x1, 0, Bess::SimEngine::PinType::output, mux_2x1, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(sel_2x1, 0, Bess::SimEngine::PinType::output, mux_2x1, 2, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(i0_2x1, true);
    simEngine_.setDigitalInput(i1_2x1, false);
    simEngine_.setDigitalInput(sel_2x1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(mux_2x1, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(sel_2x1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(mux_2x1, Bess::SimEngine::PinType::output, 0));

    // 4x1 MUX
    auto i0_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i1_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i2_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i3_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto s0_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto s1_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto mux_4x1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::MULTIPLEXER_4_1);

    simEngine_.connectComponent(i0_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i1_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i2_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 2, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i3_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 3, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(s0_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 4, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(s1_4x1, 0, Bess::SimEngine::PinType::output, mux_4x1, 5, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(i0_4x1, true);
    simEngine_.setDigitalInput(i1_4x1, false);
    simEngine_.setDigitalInput(i2_4x1, true);
    simEngine_.setDigitalInput(i3_4x1, false);

    simEngine_.setDigitalInput(s0_4x1, false);
    simEngine_.setDigitalInput(s1_4x1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(mux_4x1, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(s0_4x1, true);
    simEngine_.setDigitalInput(s1_4x1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(mux_4x1, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(s0_4x1, false);
    simEngine_.setDigitalInput(s1_4x1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(mux_4x1, Bess::SimEngine::PinType::output, 0));

    simEngine_.setDigitalInput(s0_4x1, true);
    simEngine_.setDigitalInput(s1_4x1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(mux_4x1, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, DemultiplexerLogic) {
    auto data = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto s0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto s1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto demux = simEngine_.addComponent(Bess::SimEngine::ComponentType::DEMUX_1_4);

    simEngine_.connectComponent(data, 0, Bess::SimEngine::PinType::output, demux, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(s0, 0, Bess::SimEngine::PinType::output, demux, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(s1, 0, Bess::SimEngine::PinType::output, demux, 2, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(data, true);

    bool s0_in[] = {false, true, false, true};
    bool s1_in[] = {false, false, true, true};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(s0, s0_in[i]);
        simEngine_.setDigitalInput(s1, s1_in[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (int j = 0; j < 4; j++) {
            bool expected = (i == j);
            ASSERT_EQ(simEngine_.getDigitalPinState(demux, Bess::SimEngine::PinType::output, j), expected);
        }
    }
}

TEST_F(SimulationEngineTest, DecoderLogic) {
    auto i0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto decoder = simEngine_.addComponent(Bess::SimEngine::ComponentType::DECODER_2_4);

    simEngine_.connectComponent(i0, 0, Bess::SimEngine::PinType::output, decoder, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i1, 0, Bess::SimEngine::PinType::output, decoder, 1, Bess::SimEngine::PinType::input);

    bool i0_in[] = {false, true, false, true};
    bool i1_in[] = {false, false, true, true};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(i0, i0_in[i]);
        simEngine_.setDigitalInput(i1, i1_in[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (int j = 0; j < 4; j++) {
            bool expected = (i == j);
            ASSERT_EQ(simEngine_.getDigitalPinState(decoder, Bess::SimEngine::PinType::output, j), expected);
        }
    }
}

TEST_F(SimulationEngineTest, EncoderLogic) {
    auto i0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto i3 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto encoder = simEngine_.addComponent(Bess::SimEngine::ComponentType::ENCODER_4_2);

    simEngine_.connectComponent(i0, 0, Bess::SimEngine::PinType::output, encoder, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i1, 0, Bess::SimEngine::PinType::output, encoder, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i2, 0, Bess::SimEngine::PinType::output, encoder, 2, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(i3, 0, Bess::SimEngine::PinType::output, encoder, 3, Bess::SimEngine::PinType::input);

    bool o0_expected[] = {false, true, false, true};
    bool o1_expected[] = {false, false, true, true};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(i0, i == 0);
        simEngine_.setDigitalInput(i1, i == 1);
        simEngine_.setDigitalInput(i2, i == 2);
        simEngine_.setDigitalInput(i3, i == 3);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(encoder, Bess::SimEngine::PinType::output, 0), o0_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(encoder, Bess::SimEngine::PinType::output, 1), o1_expected[i]);
    }
}

TEST_F(SimulationEngineTest, CombinationalCircuitLogic) {
    // Half Subtractor
    auto hs_a = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto hs_b = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto half_subtractor = simEngine_.addComponent(Bess::SimEngine::ComponentType::HALF_SUBTRACTOR);
    simEngine_.connectComponent(hs_a, 0, Bess::SimEngine::PinType::output, half_subtractor, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(hs_b, 0, Bess::SimEngine::PinType::output, half_subtractor, 1, Bess::SimEngine::PinType::input);

    bool hs_i1[] = {false, true, false, true};
    bool hs_i2[] = {false, false, true, true};
    bool diff_expected[] = {false, true, true, false};
    bool borrow_expected[] = {false, false, true, false};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(hs_a, hs_i1[i]);
        simEngine_.setDigitalInput(hs_b, hs_i2[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(half_subtractor, Bess::SimEngine::PinType::output, 0), diff_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(half_subtractor, Bess::SimEngine::PinType::output, 1), borrow_expected[i]);
    }

    // Full Subtractor
    auto fs_a = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto fs_b = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto fs_bin = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto full_subtractor = simEngine_.addComponent(Bess::SimEngine::ComponentType::FULL_SUBTRACTOR);
    simEngine_.connectComponent(fs_a, 0, Bess::SimEngine::PinType::output, full_subtractor, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(fs_b, 0, Bess::SimEngine::PinType::output, full_subtractor, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(fs_bin, 0, Bess::SimEngine::PinType::output, full_subtractor, 2, Bess::SimEngine::PinType::input);

    bool fs_iA[] = {false, false, false, false, true, true, true, true};
    bool fs_iB[] = {false, false, true, true, false, false, true, true};
    bool fs_iBin[] = {false, true, false, true, false, true, false, true};
    bool fs_diff_expected[] = {false, true, true, false, true, false, false, true};
    bool fs_borrow_expected[] = {false, true, true, true, false, false, false, true};

    for (int i = 0; i < 8; i++) {
        simEngine_.setDigitalInput(fs_a, fs_iA[i]);
        simEngine_.setDigitalInput(fs_b, fs_iB[i]);
        simEngine_.setDigitalInput(fs_bin, fs_iBin[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(full_subtractor, Bess::SimEngine::PinType::output, 0), fs_diff_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(full_subtractor, Bess::SimEngine::PinType::output, 1), fs_borrow_expected[i]);
    }

    // 1-bit Comparator
    auto cmp1_a = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto cmp1_b = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto comparator_1bit = simEngine_.addComponent(Bess::SimEngine::ComponentType::COMPARATOR_1_BIT);
    simEngine_.connectComponent(cmp1_a, 0, Bess::SimEngine::PinType::output, comparator_1bit, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(cmp1_b, 0, Bess::SimEngine::PinType::output, comparator_1bit, 1, Bess::SimEngine::PinType::input);

    bool cmp1_i1[] = {false, true, false, true};
    bool cmp1_i2[] = {false, false, true, true};
    bool gt_expected[] = {false, true, false, false};
    bool eq_expected[] = {true, false, false, true};
    bool lt_expected[] = {false, false, true, false};

    for (int i = 0; i < 4; i++) {
        simEngine_.setDigitalInput(cmp1_a, cmp1_i1[i]);
        simEngine_.setDigitalInput(cmp1_b, cmp1_i2[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ASSERT_EQ(simEngine_.getDigitalPinState(comparator_1bit, Bess::SimEngine::PinType::output, 0), gt_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(comparator_1bit, Bess::SimEngine::PinType::output, 1), eq_expected[i]);
        ASSERT_EQ(simEngine_.getDigitalPinState(comparator_1bit, Bess::SimEngine::PinType::output, 2), lt_expected[i]);
    }

    // Priority Encoder
    auto pe_i0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto pe_i1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto pe_i2 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto pe_i3 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto priority_encoder = simEngine_.addComponent(Bess::SimEngine::ComponentType::PRIORITY_ENCODER_4_2);
    simEngine_.connectComponent(pe_i0, 0, Bess::SimEngine::PinType::output, priority_encoder, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(pe_i1, 0, Bess::SimEngine::PinType::output, priority_encoder, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(pe_i2, 0, Bess::SimEngine::PinType::output, priority_encoder, 2, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(pe_i3, 0, Bess::SimEngine::PinType::output, priority_encoder, 3, Bess::SimEngine::PinType::input);

    simEngine_.setDigitalInput(pe_i0, true);
    simEngine_.setDigitalInput(pe_i1, false);
    simEngine_.setDigitalInput(pe_i2, false);
    simEngine_.setDigitalInput(pe_i3, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 0));
    ASSERT_FALSE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 1));

    simEngine_.setDigitalInput(pe_i1, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 0));
    ASSERT_FALSE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 1));

    simEngine_.setDigitalInput(pe_i2, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 0));
    ASSERT_TRUE(simEngine_.getDigitalPinState(priority_encoder, Bess::SimEngine::PinType::output, 1));
}

TEST_F(SimulationEngineTest, FlipFlopLogic) {
    // JK Flip Flop
    auto j = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto k = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto clk = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto clr = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto jk_ff = simEngine_.addComponent(Bess::SimEngine::ComponentType::FLIP_FLOP_JK);

    simEngine_.connectComponent(j, 0, Bess::SimEngine::PinType::output, jk_ff, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(clk, 0, Bess::SimEngine::PinType::output, jk_ff, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(k, 0, Bess::SimEngine::PinType::output, jk_ff, 2, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(clr, 0, Bess::SimEngine::PinType::output, jk_ff, 3, Bess::SimEngine::PinType::input);

    // Clear
    simEngine_.setDigitalInput(clr, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 0));
    ASSERT_TRUE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 1));
    simEngine_.setDigitalInput(clr, false);

    // J=1, K=0 -> Q=1
    simEngine_.setDigitalInput(j, true);
    simEngine_.setDigitalInput(k, false);
    simEngine_.setDigitalInput(clk, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    simEngine_.setDigitalInput(clk, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 0));

    // J=0, K=1 -> Q=0
    simEngine_.setDigitalInput(j, false);
    simEngine_.setDigitalInput(k, true);
    simEngine_.setDigitalInput(clk, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    simEngine_.setDigitalInput(clk, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 0));

    // J=1, K=1 -> Toggle
    simEngine_.setDigitalInput(j, true);
    simEngine_.setDigitalInput(k, true);
    simEngine_.setDigitalInput(clk, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    simEngine_.setDigitalInput(clk, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_TRUE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 0));
    simEngine_.setDigitalInput(clk, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    simEngine_.setDigitalInput(clk, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_FALSE(simEngine_.getDigitalPinState(jk_ff, Bess::SimEngine::PinType::output, 0));
}

TEST_F(SimulationEngineTest, Comparator2BitLogic) {
    auto a0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto a1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto b0 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto b1 = simEngine_.addComponent(Bess::SimEngine::ComponentType::INPUT);
    auto comparator = simEngine_.addComponent(Bess::SimEngine::ComponentType::COMPARATOR_2_BIT);

    simEngine_.connectComponent(a0, 0, Bess::SimEngine::PinType::output, comparator, 0, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(a1, 0, Bess::SimEngine::PinType::output, comparator, 1, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(b0, 0, Bess::SimEngine::PinType::output, comparator, 2, Bess::SimEngine::PinType::input);
    simEngine_.connectComponent(b1, 0, Bess::SimEngine::PinType::output, comparator, 3, Bess::SimEngine::PinType::input);

    for (int a = 0; a < 4; a++) {
        simEngine_.setDigitalInput(a0, a & 1);
        simEngine_.setDigitalInput(a1, a & 2);
        for (int b = 0; b < 4; b++) {
            simEngine_.setDigitalInput(b0, b & 1);
            simEngine_.setDigitalInput(b1, b & 2);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            bool gt = simEngine_.getDigitalPinState(comparator, Bess::SimEngine::PinType::output, 0);
            bool lt = simEngine_.getDigitalPinState(comparator, Bess::SimEngine::PinType::output, 1);
            bool eq = simEngine_.getDigitalPinState(comparator, Bess::SimEngine::PinType::output, 2);

            ASSERT_EQ(gt, a > b);
            ASSERT_EQ(lt, a < b);
            ASSERT_EQ(eq, a == b);
        }
    }
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