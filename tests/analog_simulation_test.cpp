#include "analog_simulation.h"
#include "gtest/gtest.h"
#include "simulation_engine.h"
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace Bess::SimEngine;

    class CustomConductance final : public AnalogComponent {
      public:
        CustomConductance(AnalogNodeId a, AnalogNodeId b, double conductanceSiemens)
            : m_a(a),
              m_b(b),
              m_conductanceSiemens(conductanceSiemens) {}

        bool validate(std::string &error, const AnalogSolveOptions &) const override {
            if (m_a == m_b) {
                error = "terminals must be different";
                return false;
            }
            if (m_conductanceSiemens <= 0.0) {
                error = "conductance must be positive";
                return false;
            }
            return true;
        }

        void stamp(AnalogStampContext &context) const override {
            context.addConductance(m_a, m_b, m_conductanceSiemens);
        }

        std::vector<AnalogNodeId> terminals() const override {
            return {m_a, m_b};
        }

        std::string name() const override {
            return "CustomConductance";
        }

      private:
        AnalogNodeId m_a;
        AnalogNodeId m_b;
        double m_conductanceSiemens;
    };
} // namespace

TEST(AnalogSimulationTest, ResistorObeysOhmsLawWithVoltageSource) {
    AnalogCircuit circuit;
    const auto node = circuit.createNode("vout");
    circuit.addVoltageSource(node, AnalogGroundNode, 10.0, "V1");
    circuit.addResistor(node, AnalogGroundNode, 1000.0, "R1");

    const auto solution = circuit.solve();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NEAR(solution.voltage(node), 10.0, 1e-9);
    ASSERT_TRUE(solution.branchCurrent("V1").has_value());
    EXPECT_NEAR(*solution.branchCurrent("V1"), -0.01, 1e-12);
}

TEST(AnalogSimulationTest, ResistorDividerSolvesNodeVoltage) {
    AnalogCircuit circuit;
    const auto vin = circuit.createNode("vin");
    const auto vout = circuit.createNode("vout");
    circuit.addVoltageSource(vin, AnalogGroundNode, 10.0, "V1");
    circuit.addResistor(vin, vout, 1000.0, "RTop");
    circuit.addResistor(vout, AnalogGroundNode, 1000.0, "RBottom");

    const auto solution = circuit.solve();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NEAR(solution.voltage(vin), 10.0, 1e-9);
    EXPECT_NEAR(solution.voltage(vout), 5.0, 1e-9);
}

TEST(AnalogSimulationTest, RejectsInvalidResistorValue) {
    AnalogCircuit circuit;
    const auto node = circuit.createNode("n1");
    circuit.addResistor(node, AnalogGroundNode, 0.0, "RBad");

    const auto solution = circuit.solve();

    EXPECT_EQ(solution.status, AnalogSolveStatus::invalidComponent);
    EXPECT_NE(solution.message.find("RBad"), std::string::npos);
}

TEST(AnalogSimulationTest, ReportsFloatingResistiveNetworkAsSingular) {
    AnalogCircuit circuit;
    const auto a = circuit.createNode("a");
    const auto b = circuit.createNode("b");
    circuit.addResistor(a, b, 1000.0, "RFloating");

    const auto solution = circuit.solve();

    EXPECT_EQ(solution.status, AnalogSolveStatus::singularMatrix);
}

TEST(AnalogSimulationTest, AcceptsCustomAnalogComponentsThroughStampingInterface) {
    AnalogCircuit circuit;
    const auto node = circuit.createNode("custom");
    circuit.addCurrentSource(AnalogGroundNode, node, 0.001, "I1");
    circuit.addComponent(std::make_shared<CustomConductance>(node, AnalogGroundNode, 0.001));

    const auto solution = circuit.solve();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NEAR(solution.voltage(node), 1.0, 1e-9);
}

TEST(AnalogSimulationTest, SimulationEngineOwnsReusableAnalogCircuit) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clearAnalogCircuit();

    auto &circuit = engine.getAnalogCircuit();
    const auto node = circuit.createNode("engine-node");
    circuit.addVoltageSource(node, AnalogGroundNode, 3.3, "VEngine");
    circuit.addResistor(node, AnalogGroundNode, 330.0, "REngine");

    const auto solution = engine.solveAnalogCircuit();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NEAR(solution.voltage(node), 3.3, 1e-9);

    engine.clearAnalogCircuit();
}
