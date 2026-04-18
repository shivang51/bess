#include "analog_simulation.h"
#include "component_catalog.h"
#include "gtest/gtest.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "simulation_engine.h"
#include <cmath>
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

TEST(AnalogSimulationTest, FloatingSourceResistorNetworkAutoReferencesAndSolves) {
    AnalogCircuit circuit;
    const auto a = circuit.createNode("a");
    const auto b = circuit.createNode("b");

    circuit.addVoltageSource(a, b, 10.0, "VF");
    circuit.addResistor(a, b, 1000.0, "RF");

    const auto solution = circuit.solve();

    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NE(solution.message.find("auto-referenced"), std::string::npos);
    ASSERT_TRUE(solution.branchCurrent("VF").has_value());
    EXPECT_NEAR(*solution.branchCurrent("VF"), -0.01, 1e-12);
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

TEST(AnalogSimulationTest, SupportsDynamicTerminalConnectionsAndComponentStateQueries) {
    AnalogCircuit circuit;
    const auto rail = circuit.createNode("rail");
    const auto supply = circuit.addVoltageSource(10.0, "V1");
    const auto resistor = circuit.addResistor(1000.0, "R1");

    const auto disconnectedSolve = circuit.solve();
    EXPECT_TRUE(disconnectedSolve.ok()) << disconnectedSolve.message;

    ASSERT_TRUE(circuit.connectTerminal(supply, 0, rail));
    ASSERT_TRUE(circuit.connectTerminal(supply, 1, AnalogGroundNode));
    ASSERT_TRUE(circuit.connectTerminal(resistor, 0, rail));
    ASSERT_TRUE(circuit.connectTerminal(resistor, 1, AnalogGroundNode));

    const auto solution = circuit.solve();

    ASSERT_TRUE(solution.ok()) << solution.message;
    const auto resistorState = circuit.getComponentState(resistor);
    ASSERT_EQ(resistorState.terminals.size(), 2u);
    EXPECT_FALSE(resistorState.simError);
    EXPECT_EQ(resistorState.terminals[0].node, rail);
    EXPECT_EQ(resistorState.terminals[1].node, AnalogGroundNode);
    EXPECT_NEAR(resistorState.terminals[0].voltage, 10.0, 1e-9);
    EXPECT_NEAR(resistorState.terminals[1].voltage, 0.0, 1e-12);
    EXPECT_NEAR(resistorState.terminals[0].current, 0.01, 1e-12);
    EXPECT_NEAR(resistorState.terminals[1].current, -0.01, 1e-12);

    ASSERT_TRUE(circuit.disconnectTerminal(resistor, 1));
    const auto partiallyConnectedSolution = circuit.solve();
    EXPECT_TRUE(partiallyConnectedSolution.ok()) << partiallyConnectedSolution.message;

    const auto disconnectedState = circuit.getComponentState(resistor);
    ASSERT_EQ(disconnectedState.terminals.size(), 2u);
    EXPECT_FALSE(disconnectedState.simError);
    EXPECT_TRUE(disconnectedState.terminals[0].connected);
    EXPECT_FALSE(disconnectedState.terminals[1].connected);
    EXPECT_NEAR(disconnectedState.terminals[0].current, 0.0, 1e-12);
    EXPECT_NEAR(disconnectedState.terminals[1].current, 0.0, 1e-12);
}

TEST(AnalogSimulationTest, DisconnectedResistorIsIgnoredWhenOtherCircuitIsValid) {
    AnalogCircuit circuit;
    const auto high = circuit.createNode("high");
    const auto low = circuit.createNode("low");

    circuit.addVoltageSource(high, low, 5.0, "V1");
    circuit.addResistor(high, low, 1000.0, "RConnected");
    const auto disconnectedResistor = circuit.addResistor(1000.0, "RDisconnected");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;
    ASSERT_TRUE(solution.branchCurrent("V1").has_value());
    EXPECT_NEAR(*solution.branchCurrent("V1"), -0.005, 1e-12);

    const auto disconnectedState = circuit.getComponentState(disconnectedResistor);
    ASSERT_EQ(disconnectedState.terminals.size(), 2u);
    EXPECT_FALSE(disconnectedState.simError);
    EXPECT_FALSE(disconnectedState.terminals[0].connected);
    EXPECT_FALSE(disconnectedState.terminals[1].connected);
}

TEST(AnalogSimulationTest, ResistorWithSameNodeTerminalsIsIgnoredBySolveValidation) {
    AnalogCircuit circuit;
    const auto high = circuit.createNode("high");

    circuit.addVoltageSource(high, AnalogGroundNode, 5.0, "V1");
    const auto shortedResistor = circuit.addResistor(high, high, 1000.0, "RShorted");
    circuit.addResistor(high, AnalogGroundNode, 1000.0, "RLoad");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;
    ASSERT_TRUE(solution.branchCurrent("V1").has_value());
    EXPECT_NEAR(*solution.branchCurrent("V1"), -0.005, 1e-12);

    const auto shortedState = circuit.getComponentState(shortedResistor);
    ASSERT_EQ(shortedState.terminals.size(), 2u);
    EXPECT_FALSE(shortedState.simError);
    EXPECT_TRUE(shortedState.terminals[0].connected);
    EXPECT_TRUE(shortedState.terminals[1].connected);
    EXPECT_NEAR(shortedState.terminals[0].current, 0.0, 1e-12);
    EXPECT_NEAR(shortedState.terminals[1].current, 0.0, 1e-12);
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

TEST(AnalogSimulationTest, SimulationEngineProvidesAnalogComponentApi) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clearAnalogCircuit();

    const auto node = engine.getAnalogCircuit().createNode("api-node");
    engine.getAnalogCircuit().addVoltageSource(node, AnalogGroundNode, 5.0, "VApi");
    const auto resistor = engine.addAnalogResistor(1000.0, "RApi");

    ASSERT_TRUE(engine.connectAnalogTerminal(resistor, 0, node));
    ASSERT_TRUE(engine.connectAnalogTerminal(resistor, 1, AnalogGroundNode));

    const auto solution = engine.solveAnalogCircuit();
    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto state = engine.getAnalogComponentState(resistor);
    ASSERT_EQ(state.terminals.size(), 2u);
    EXPECT_NEAR(state.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(state.terminals[0].current, 0.005, 1e-12);

    engine.clearAnalogCircuit();
}

TEST(AnalogSimulationTest, SimulationEngineAnalogSlotStateReflectsCurrentFlow) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    auto &circuit = engine.getAnalogCircuit();
    const auto node = circuit.createNode("flow-node");
    const auto source = circuit.addVoltageSource(node, AnalogGroundNode, 5.0, "VFlow");

    // Open-circuit voltage source has terminal voltage but zero branch current.
    const auto openCircuitSolution = engine.solveAnalogCircuit();
    ASSERT_TRUE(openCircuitSolution.ok()) << openCircuitSolution.message;
    EXPECT_EQ(engine.getSlotState(source, SlotType::analogTerminal, 0).state, LogicState::low);
    EXPECT_EQ(engine.getSlotState(source, SlotType::analogTerminal, 1).state, LogicState::low);

    const auto resistor = engine.addAnalogResistor(1000.0, "RFlow");
    ASSERT_TRUE(engine.connectSlots({source, 0, SlotType::analogTerminal},
                                    {resistor, 0, SlotType::analogTerminal}));
    ASSERT_TRUE(engine.connectSlots({source, 1, SlotType::analogTerminal},
                                    {resistor, 1, SlotType::analogTerminal}));

    const auto loadedSolution = engine.solveAnalogCircuit();
    ASSERT_TRUE(loadedSolution.ok()) << loadedSolution.message;
    EXPECT_EQ(engine.getSlotState(source, SlotType::analogTerminal, 0).state, LogicState::high);
    EXPECT_EQ(engine.getSlotState(source, SlotType::analogTerminal, 1).state, LogicState::high);

    engine.clear();
}

TEST(AnalogSimulationTest, SimulationEngineUnifiedSlotApiSupportsAnalogConnections) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto resistorA = engine.addAnalogResistor(1000.0, "RA");
    const auto resistorB = engine.addAnalogResistor(2000.0, "RB");

    const SimulationEngine::SlotEndpoint aTerminal0{resistorA, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint bTerminal0{resistorB, 0, SlotType::analogTerminal};

    const auto [canConnect, error] = engine.canConnectSlots(aTerminal0, bTerminal0);
    ASSERT_TRUE(canConnect) << error;
    ASSERT_TRUE(engine.connectSlots(aTerminal0, bTerminal0));
    EXPECT_TRUE(engine.isSlotConnected(resistorA, SlotType::analogTerminal, 0));
    EXPECT_TRUE(engine.isSlotConnected(resistorB, SlotType::analogTerminal, 0));

    ASSERT_TRUE(engine.disconnectSlots(aTerminal0, bTerminal0));
    EXPECT_FALSE(engine.isSlotConnected(resistorA, SlotType::analogTerminal, 0));
    EXPECT_FALSE(engine.isSlotConnected(resistorB, SlotType::analogTerminal, 0));

    engine.clearAnalogCircuit();
}

TEST(AnalogSimulationTest, SimulationEngineUnifiedSlotApiRejectsMixedSignalConnections) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto resistor = engine.addAnalogResistor(1000.0, "RMixed");
    const auto inputDef = ComponentCatalog::instance().findDefByName("Input");
    ASSERT_NE(inputDef, nullptr);
    const auto digitalInput = engine.addComponent(inputDef, false);

    const SimulationEngine::SlotEndpoint analogEndpoint{resistor, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint digitalEndpoint{digitalInput, 0, SlotType::digitalOutput};

    const auto [canConnect, error] = engine.canConnectSlots(analogEndpoint, digitalEndpoint);
    EXPECT_FALSE(canConnect);
    EXPECT_EQ(error, "Cannot connect analog terminals to digital pins");
    EXPECT_FALSE(engine.connectSlots(analogEndpoint, digitalEndpoint));

    engine.clear();
}

TEST(AnalogSimulationTest, SimulationEngineUnifiedSlotApiSolvesGroundedSourceResistorCircuit) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto sourceDef = ComponentCatalog::instance().findDefByName("DC Voltage Source");
    const auto resistorDef = ComponentCatalog::instance().findDefByName("Resistor");
    const auto groundDef = ComponentCatalog::instance().findDefByName("Ground");
    ASSERT_NE(sourceDef, nullptr);
    ASSERT_NE(resistorDef, nullptr);
    ASSERT_NE(groundDef, nullptr);

    const auto sourceTrait = sourceDef->getTrait<AnalogComponentTrait>();
    const auto resistorTrait = resistorDef->getTrait<AnalogComponentTrait>();
    const auto groundTrait = groundDef->getTrait<AnalogComponentTrait>();
    ASSERT_NE(sourceTrait, nullptr);
    ASSERT_NE(sourceTrait->factory, nullptr);
    ASSERT_NE(resistorTrait, nullptr);
    ASSERT_NE(resistorTrait->factory, nullptr);
    ASSERT_NE(groundTrait, nullptr);
    ASSERT_NE(groundTrait->factory, nullptr);

    const auto source = engine.addAnalogComponent(sourceTrait->factory(), sourceDef);
    const auto resistor = engine.addAnalogComponent(resistorTrait->factory(), resistorDef);
    const auto ground = engine.addAnalogComponent(groundTrait->factory(), groundDef);

    const SimulationEngine::SlotEndpoint sourcePlus{source, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint sourceMinus{source, 1, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint resistorPlus{resistor, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint resistorMinus{resistor, 1, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint groundPin{ground, 0, SlotType::analogTerminal};

    ASSERT_TRUE(engine.connectSlots(sourceMinus, groundPin));
    ASSERT_TRUE(engine.connectSlots(sourcePlus, resistorPlus));
    ASSERT_TRUE(engine.connectSlots(sourceMinus, resistorMinus));

    const auto solution = engine.solveAnalogCircuit();
    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto sourceState = engine.getAnalogComponentState(source);
    ASSERT_EQ(sourceState.terminals.size(), 2u);
    EXPECT_TRUE(sourceState.terminals[0].connected);
    EXPECT_TRUE(sourceState.terminals[1].connected);
    EXPECT_NEAR(sourceState.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(sourceState.terminals[1].voltage, 0.0, 1e-9);

    const auto resistorState = engine.getAnalogComponentState(resistor);
    ASSERT_EQ(resistorState.terminals.size(), 2u);
    EXPECT_NEAR(resistorState.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(resistorState.terminals[1].voltage, 0.0, 1e-9);
    EXPECT_NEAR(resistorState.terminals[0].current, 0.005, 1e-12);
    EXPECT_NEAR(resistorState.terminals[1].current, -0.005, 1e-12);

    engine.clear();
}

TEST(AnalogSimulationTest, SimulationEngineUnifiedSlotApiAutoReferencesFloatingSourceResistorCircuit) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto sourceDef = ComponentCatalog::instance().findDefByName("DC Voltage Source");
    const auto resistorDef = ComponentCatalog::instance().findDefByName("Resistor");
    ASSERT_NE(sourceDef, nullptr);
    ASSERT_NE(resistorDef, nullptr);

    const auto sourceTrait = sourceDef->getTrait<AnalogComponentTrait>();
    const auto resistorTrait = resistorDef->getTrait<AnalogComponentTrait>();
    ASSERT_NE(sourceTrait, nullptr);
    ASSERT_NE(sourceTrait->factory, nullptr);
    ASSERT_NE(resistorTrait, nullptr);
    ASSERT_NE(resistorTrait->factory, nullptr);

    const auto source = engine.addAnalogComponent(sourceTrait->factory(), sourceDef);
    const auto resistor = engine.addAnalogComponent(resistorTrait->factory(), resistorDef);

    const SimulationEngine::SlotEndpoint sourcePlus{source, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint sourceMinus{source, 1, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint resistorPlus{resistor, 0, SlotType::analogTerminal};
    const SimulationEngine::SlotEndpoint resistorMinus{resistor, 1, SlotType::analogTerminal};

    ASSERT_TRUE(engine.connectSlots(sourcePlus, resistorPlus));
    ASSERT_TRUE(engine.connectSlots(sourceMinus, resistorMinus));

    const auto solution = engine.solveAnalogCircuit();
    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NE(solution.message.find("auto-referenced"), std::string::npos);

    const auto resistorState = engine.getAnalogComponentState(resistor);
    ASSERT_EQ(resistorState.terminals.size(), 2u);
    EXPECT_NEAR(resistorState.terminals[0].current, 0.005, 1e-12);
    EXPECT_NEAR(resistorState.terminals[1].current, -0.005, 1e-12);

    engine.clear();
}

TEST(AnalogSimulationTest, AnalogCatalogIncludesSourceAndProbeComponents) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    struct CatalogExpectation {
        std::string name;
        size_t expectedTerminalCount;
    };

    const std::vector<CatalogExpectation> expectations = {
        {"DC Voltage Source", 2},
        {"Analog Test Point", 1},
        {"Differential Voltage Probe", 2},
        {"Inline Current Probe", 2},
        {"Ground", 1},
    };

    for (const auto &item : expectations) {
        const auto def = ComponentCatalog::instance().findDefByName(item.name);
        ASSERT_NE(def, nullptr) << item.name;
        const auto analogTrait = def->getTrait<AnalogComponentTrait>();
        ASSERT_NE(analogTrait, nullptr) << item.name;
        EXPECT_EQ(analogTrait->terminalCount, item.expectedTerminalCount) << item.name;
    }
}

TEST(AnalogSimulationTest, AnalogProbeComponentsExposeCurrentAndVoltageMeasurements) {
    AnalogCircuit circuit;

    const auto vin = circuit.createNode("vin");
    const auto mid = circuit.createNode("mid");

    circuit.addVoltageSource(vin, AnalogGroundNode, 10.0, "V1");
    const auto currentProbeId = circuit.addCurrentProbe(vin, mid, "IP");
    circuit.addResistor(mid, AnalogGroundNode, 1000.0, "R1");
    const auto voltageProbeId = circuit.addVoltageProbe(mid, AnalogGroundNode, "VP");
    const auto testPointId = circuit.addTestPoint(mid, "TP");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto currentState = circuit.getComponentState(currentProbeId);
    ASSERT_EQ(currentState.terminals.size(), 2u);
    EXPECT_NEAR(currentState.terminals[0].current, 0.01, 1e-12);
    EXPECT_NEAR(currentState.terminals[1].current, -0.01, 1e-12);

    const auto voltageState = circuit.getComponentState(voltageProbeId);
    ASSERT_EQ(voltageState.terminals.size(), 2u);
    const auto measuredV = voltageState.terminals[0].voltage - voltageState.terminals[1].voltage;
    EXPECT_NEAR(measuredV, 10.0, 1e-9);

    const auto testPointState = circuit.getComponentState(testPointId);
    ASSERT_EQ(testPointState.terminals.size(), 1u);
    EXPECT_TRUE(testPointState.terminals[0].connected);
    EXPECT_NEAR(testPointState.terminals[0].voltage, 10.0, 1e-9);
}

TEST(AnalogSimulationTest, DisconnectedVoltageProbeIsIgnoredBySolveValidation) {
    AnalogCircuit circuit;

    const auto a = circuit.createNode("a");
    const auto b = circuit.createNode("b");
    circuit.addVoltageSource(a, b, 5.0, "V1");
    circuit.addResistor(a, b, 1000.0, "R1");
    circuit.addGroundReference(b, "GND");

    const auto disconnectedProbe = circuit.addVoltageProbe("VProbe");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto probeState = circuit.getComponentState(disconnectedProbe);
    ASSERT_EQ(probeState.terminals.size(), 2u);
    EXPECT_FALSE(probeState.terminals[0].connected);
    EXPECT_FALSE(probeState.terminals[1].connected);
}

TEST(AnalogSimulationTest, DisconnectedCurrentProbeIsIgnoredBySolveValidation) {
    AnalogCircuit circuit;

    const auto a = circuit.createNode("a");
    const auto b = circuit.createNode("b");
    circuit.addVoltageSource(a, b, 5.0, "V1");
    circuit.addResistor(a, b, 1000.0, "R1");
    circuit.addGroundReference(b, "GND");

    const auto disconnectedProbe = circuit.addCurrentProbe("IProbe");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;

    const auto probeState = circuit.getComponentState(disconnectedProbe);
    ASSERT_EQ(probeState.terminals.size(), 2u);
    EXPECT_FALSE(probeState.terminals[0].connected);
    EXPECT_FALSE(probeState.terminals[1].connected);
}

TEST(AnalogSimulationTest, SimulationEngineExposesResistorValueForSelectedResistor) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto resistor = engine.addAnalogResistor(2200.0, "RView");
    const auto sourceDef = ComponentCatalog::instance().findDefByName("DC Voltage Source");
    ASSERT_NE(sourceDef, nullptr);
    const auto sourceTrait = sourceDef->getTrait<AnalogComponentTrait>();
    ASSERT_NE(sourceTrait, nullptr);
    ASSERT_NE(sourceTrait->factory, nullptr);
    const auto source = engine.addAnalogComponent(sourceTrait->factory(), sourceDef);

    const auto resistorValue = engine.getAnalogResistorValue(resistor);
    ASSERT_TRUE(resistorValue.has_value());
    EXPECT_NEAR(*resistorValue, 2200.0, 1e-12);

    EXPECT_FALSE(engine.getAnalogResistorValue(source).has_value());

    engine.clear();
}

TEST(AnalogSimulationTest, GroundReferenceAnchorsConnectedNodeToZeroVolts) {
    AnalogCircuit circuit;

    const auto a = circuit.createNode("a");
    const auto b = circuit.createNode("b");
    circuit.addVoltageSource(a, b, 10.0, "VG");
    circuit.addResistor(a, b, 1000.0, "RG");
    const auto ground = circuit.addGroundReference(b, "GND");

    const auto solution = circuit.solve();
    ASSERT_TRUE(solution.ok()) << solution.message;
    EXPECT_NEAR(solution.voltage(b), 0.0, 1e-9);
    EXPECT_NEAR(solution.voltage(a), 10.0, 1e-9);

    const auto groundState = circuit.getComponentState(ground);
    ASSERT_EQ(groundState.terminals.size(), 1u);
    EXPECT_NEAR(groundState.terminals[0].voltage, 0.0, 1e-9);
}

TEST(AnalogSimulationTest, AnalogMeasurementsAutoSolveWhenQueried) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clearAnalogCircuit();

    auto &circuit = engine.getAnalogCircuit();
    const auto node = circuit.createNode("measure");
    circuit.addVoltageSource(node, AnalogGroundNode, 5.0, "VMeasure");
    const auto resistor = circuit.addResistor(node, AnalogGroundNode, 1000.0, "RMeasure");

    // Intentionally do not call solveAnalogCircuit() here.
    const auto state = engine.getAnalogComponentState(resistor);

    ASSERT_FALSE(state.simError) << state.errorMessage;
    ASSERT_EQ(state.terminals.size(), 2u);
    EXPECT_TRUE(std::isfinite(state.terminals[0].voltage));
    EXPECT_TRUE(std::isfinite(state.terminals[0].current));
    EXPECT_NEAR(state.terminals[0].voltage, 5.0, 1e-9);
    EXPECT_NEAR(state.terminals[0].current, 0.005, 1e-12);
}

TEST(AnalogSimulationTest, ResistorDefinitionIsCatalogBackedAndCreatesAnalogSceneSlots) {
    auto &engine = Bess::SimEngine::SimulationEngine::instance();
    engine.clear();

    const auto resistorDef = ComponentCatalog::instance().findDefByName("Resistor");
    ASSERT_NE(resistorDef, nullptr);
    ASSERT_NE(resistorDef->getTrait<AnalogComponentTrait>(), nullptr);

    const auto created = Bess::Canvas::SimulationSceneComponent::createNew(resistorDef);
    ASSERT_EQ(created.size(), 3u);

    const auto resistorSceneComp = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(created[0]);
    const auto terminalA = std::dynamic_pointer_cast<Bess::Canvas::SlotSceneComponent>(created[1]);
    const auto terminalB = std::dynamic_pointer_cast<Bess::Canvas::SlotSceneComponent>(created[2]);

    ASSERT_NE(resistorSceneComp, nullptr);
    ASSERT_NE(terminalA, nullptr);
    ASSERT_NE(terminalB, nullptr);
    EXPECT_TRUE(resistorSceneComp->isAnalogComponent());
    EXPECT_TRUE(terminalA->isAnalogSlot());
    EXPECT_TRUE(terminalB->isAnalogSlot());
    EXPECT_EQ(terminalA->getName(), "+");
    EXPECT_EQ(terminalB->getName(), "-");
}
