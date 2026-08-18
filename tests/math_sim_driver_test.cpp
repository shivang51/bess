#include "common/types.h"
#include "component_catalog.h"
#include "math_sim_driver.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using Bess::TimeNs;
    using Bess::UUID;
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers::Math;

    PortRef
    scalarPort(const UUID &componentId, PortDirection direction, int index) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = SignalKind::scalar,
                .index = index};
    }

    PortRef
    digitalPort(const UUID &componentId, PortDirection direction, int index) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = SignalKind::digital,
                .index = index};
    }

    PortRef
    stringPort(const UUID &componentId, PortDirection direction, int index) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = SignalKind::string,
                .index = index};
    }

    UUID addMathComponent(MathSimDriver &driver,
                          const std::shared_ptr<MathCompDef> &definition,
                          bool scheduleSim = false) {
        const auto component = std::dynamic_pointer_cast<MathSimComp>(
            driver.createComp(definition, false));
        EXPECT_NE(component, nullptr);
        if (!component) {
            return UUID::null;
        }

        return driver.addComponent(component, scheduleSim);
    }

    std::shared_ptr<MathCompDef>
    makeMathTestDef(const std::string &name,
                    size_t inputCount,
                    size_t outputCount,
                    const MathCompDef::TMathSimFn &simFn,
                    bool autoReschedule = false,
                    TimeNs autoRescheduleDelay = TimeNs(1000000)) {
        const auto definition = std::make_shared<MathCompDef>();
        definition->setName(name);
        definition->setGroupName("Test");
        definition->setInputPortDescriptor({
            .direction = PortDirection::input,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = inputCount,
        });
        definition->setOutputPortDescriptor({
            .direction = PortDirection::output,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = outputCount,
        });
        definition->setSimFn(simFn);
        definition->setAutoReschedule(autoReschedule);
        definition->setAutoRescheduleDelay(autoRescheduleDelay);
        return definition;
    }

    template <typename TDone>
    bool runDriverUntil(MathSimDriver &driver, const TDone &done) {
        std::thread runThread([&driver]() { driver.run(); });

        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
        while (!done() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!done()) {
            driver.stop();
        }

        runThread.join();
        return done();
    }
} // namespace

TEST(MathSimDriverTest, BuiltInBinaryOpsProduceScalarOutputs) {
    MathSimDriver driver;
    driver.init();

    const auto addId = addMathComponent(
        driver, MathCompDef::makeBinaryOp("Add Test", "Math", MathOpKind::add));
    ASSERT_NE(addId, UUID::null);

    const auto subId =
        addMathComponent(driver,
                         MathCompDef::makeBinaryOp(
                             "Subtract Test", "Math", MathOpKind::subtract));
    ASSERT_NE(subId, UUID::null);

    EXPECT_TRUE(
        driver.simulate({UUID(1), addId, UUID::null, TimeNs(0)},
                        {PortState::scalar(2.5), PortState::scalar(4.0)}));
    auto addOutput =
        driver.getPortState(scalarPort(addId, PortDirection::output, 0));
    EXPECT_TRUE(addOutput.isScalar());
    EXPECT_DOUBLE_EQ(addOutput.scalarValue, 6.5);

    EXPECT_TRUE(
        driver.simulate({UUID(2), subId, UUID::null, TimeNs(0)},
                        {PortState::scalar(7.0), PortState::scalar(2.25)}));
    auto subOutput =
        driver.getPortState(scalarPort(subId, PortDirection::output, 0));
    EXPECT_TRUE(subOutput.isScalar());
    EXPECT_DOUBLE_EQ(subOutput.scalarValue, 4.75);
}

TEST(MathSimDriverTest, DefinitionDefaultsInitializeComponentInputStates) {
    const auto definition = makeMathTestDef(
        "Default Input Test",
        3,
        1,
        [](const std::shared_ptr<MathCompSimData> &data) { return data; });
    auto inputDescriptor = definition->getInputPortDescriptor();
    inputDescriptor.defaultStates = {PortState::scalar(2.5),
                                     PortState::scalar(-4.0)};
    definition->setInputPortDescriptor(inputDescriptor);

    const auto component = std::dynamic_pointer_cast<MathSimComp>(
        MathSimComp::fromDef(definition));
    ASSERT_NE(component, nullptr);

    const auto &states = component->getInputStates();
    ASSERT_EQ(states.size(), 3);
    EXPECT_DOUBLE_EQ(states[0].scalarValue, 2.5);
    EXPECT_DOUBLE_EQ(states[1].scalarValue, -4.0);
    EXPECT_DOUBLE_EQ(states[2].scalarValue, 0.0);
}

TEST(MathSimDriverTest,
     MixedPortDescriptorsPreservePerPortMetadataAndDefaults) {
    MathCompDef definition;
    definition.setInputPortDescriptor({
        .direction = PortDirection::input,
        .ports = {{.name = "Value",
                   .signalKind = SignalKind::scalar,
                   .quantityKind = QuantityKind::dimensionless,
                   .defaultState = PortState::scalar(2.5)},
                  {.name = "Variable",
                   .signalKind = SignalKind::string,
                   .quantityKind = QuantityKind::none,
                   .defaultState = PortState::string("t")}},
    });

    const auto descriptor = definition.getInputPortDescriptor();
    ASSERT_EQ(descriptor.portCount(), 2u);
    EXPECT_EQ(descriptor.signalKindAt(0), SignalKind::scalar);
    EXPECT_EQ(descriptor.signalKindAt(1), SignalKind::string);
    EXPECT_EQ(descriptor.nameAt(0), "Value");
    EXPECT_EQ(descriptor.nameAt(1), "Variable");

    const auto states = descriptor.makeInitialStates();
    ASSERT_EQ(states.size(), 2u);
    EXPECT_TRUE(states[0].isScalar());
    EXPECT_DOUBLE_EQ(states[0].scalarValue, 2.5);
    EXPECT_TRUE(states[1].isString());
    EXPECT_EQ(states[1].stringValue, "t");

    MathCompDef restored;
    restored.loadJson(definition.toJson());
    const auto restoredDescriptor = restored.getInputPortDescriptor();
    ASSERT_EQ(restoredDescriptor.portCount(), 2u);
    EXPECT_EQ(restoredDescriptor.signalKindAt(0), SignalKind::scalar);
    EXPECT_EQ(restoredDescriptor.signalKindAt(1), SignalKind::string);
    EXPECT_EQ(restoredDescriptor.makeInitialStates()[1].stringValue, "t");
}

TEST(MathSimDriverTest, AddingComponentImmediatelyStampsItsInitialState) {
    MathSimDriver driver;
    driver.init();

    const auto definition = makeMathTestDef(
        "Initially Stamped Component",
        0,
        1,
        [](const std::shared_ptr<MathCompSimData> &data) { return data; });
    auto outputDescriptor = definition->getOutputPortDescriptor();
    outputDescriptor.defaultStates = {PortState::scalar(7.5)};
    definition->setOutputPortDescriptor(outputDescriptor);

    const auto componentId = addMathComponent(driver, definition);
    ASSERT_NE(componentId, UUID::null);

    const auto stampData = driver.getStampData();
    const auto history = stampData.find(componentId);
    ASSERT_TRUE(history.has_value());
    ASSERT_EQ(history->samples.size(), 1);
    EXPECT_EQ(history->samples[0].simTime, TimeNs(0));
    ASSERT_EQ(history->samples[0].outputStates.size(), 1);
    EXPECT_DOUBLE_EQ(history->samples[0].outputStates[0].scalarValue, 7.5);
}

TEST(MathSimDriverTest, DefinitionSerializationPreservesPortDefaults) {
    MathCompDef definition;
    definition.setInputPortDescriptor({
        .direction = PortDirection::input,
        .signalKind = SignalKind::scalar,
        .quantityKind = QuantityKind::dimensionless,
        .count = 2,
        .defaultStates = {PortState::scalar(1.25), PortState::scalar(3.5)},
    });

    const auto json = definition.toJson();
    ASSERT_TRUE(json["inputPorts"]["defaultStates"].isArray());

    MathCompDef restored;
    restored.loadJson(json);
    const auto descriptor = restored.getInputPortDescriptor();
    ASSERT_EQ(descriptor.defaultStates.size(), 2);
    EXPECT_DOUBLE_EQ(descriptor.defaultStates[0].scalarValue, 1.25);
    EXPECT_DOUBLE_EQ(descriptor.defaultStates[1].scalarValue, 3.5);
}

TEST(MathSimDriverTest, BuiltInSineStartsWithUnitFrequencyAndAmplitude) {
    MathSimDriver driver;
    driver.init();

    const auto definition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Sine (sin((f*t) + p) * a)");
    ASSERT_NE(definition, nullptr);

    const auto component =
        std::dynamic_pointer_cast<MathSimComp>(driver.createComp(definition));
    ASSERT_NE(component, nullptr);

    const auto &states = component->getInputStates();
    ASSERT_EQ(states.size(), 3);
    EXPECT_DOUBLE_EQ(states[0].scalarValue, 1.0);
    EXPECT_DOUBLE_EQ(states[1].scalarValue, 0.0);
    EXPECT_DOUBLE_EQ(states[2].scalarValue, 1.0);
}

TEST(MathSimDriverTest, DefinitionSerializationPreservesEveryOperationKind) {
    const std::vector<std::pair<MathOpKind, std::string>> cases = {
        {MathOpKind::none, "none"},
        {MathOpKind::add, "add"},
        {MathOpKind::subtract, "subtract"},
        {MathOpKind::multiply, "multiply"},
        {MathOpKind::pow, "pow"},
    };

    for (const auto &[kind, serializedKind] : cases) {
        SCOPED_TRACE(serializedKind);
        auto definition =
            MathCompDef::makeBinaryOp("Serialization Test", "Math", kind);

        const auto json = definition->toJson();
        ASSERT_TRUE(json["opKind"].isString());
        EXPECT_EQ(json["opKind"].asString(), serializedKind);

        MathCompDef restored;
        restored.loadJson(json);
        EXPECT_EQ(restored.getOpKind(), kind);
    }
}

TEST(MathSimDriverTest, DefinitionSerializationPreservesEventDelays) {
    constexpr double autoRescheduleDelayNs = 1234567.5;
    constexpr double propagationDelayNs = 7654321.25;

    MathCompDef definition;
    definition.setAutoReschedule(true);
    definition.setAutoRescheduleDelay(TimeNs(autoRescheduleDelayNs));
    definition.setPropDelay(TimeNs(propagationDelayNs));

    const auto json = definition.toJson();
    ASSERT_TRUE(json["autoRescheduleDelay"].isNumeric());
    EXPECT_DOUBLE_EQ(json["autoRescheduleDelay"].asDouble(),
                     autoRescheduleDelayNs);

    MathCompDef restored;
    restored.loadJson(json);
    EXPECT_TRUE(restored.getAutoReschedule());
    EXPECT_DOUBLE_EQ(restored.getAutoRescheduleDelay().count(),
                     autoRescheduleDelayNs);
    EXPECT_DOUBLE_EQ(restored.getPropDelay().count(), propagationDelayNs);
}

TEST(MathSimDriverTest, MissingAutoRescheduleDelayUsesDefinitionDefault) {
    MathCompDef definition;
    auto json = definition.toJson();
    json.removeMember("autoRescheduleDelay");

    MathCompDef restored;
    restored.loadJson(json);
    EXPECT_DOUBLE_EQ(restored.getAutoRescheduleDelay().count(), 0.0);
}

TEST(MathSimDriverTest, ConnectionsCollapseScalarInputsForEventSimulation) {
    MathSimDriver driver;
    driver.init();

    const auto sourceDef =
        MathCompDef::makeBinaryOp("Source", "Math", MathOpKind::add);
    const auto targetDef =
        MathCompDef::makeBinaryOp("Target", "Math", MathOpKind::add);

    const auto sourceA = addMathComponent(driver, sourceDef);
    const auto sourceB = addMathComponent(driver, sourceDef);
    const auto target = addMathComponent(driver, targetDef);
    ASSERT_NE(sourceA, UUID::null);
    ASSERT_NE(sourceB, UUID::null);
    ASSERT_NE(target, UUID::null);

    ASSERT_TRUE(driver.setOutputPortState(sourceA, 0, PortState::scalar(5.0)));
    ASSERT_TRUE(driver.setOutputPortState(sourceB, 0, PortState::scalar(2.0)));

    ASSERT_TRUE(
        driver.connectPorts(scalarPort(sourceA, PortDirection::output, 0),
                            scalarPort(target, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(sourceB, PortDirection::output, 0),
                            scalarPort(target, PortDirection::input, 1),
                            false));

    const auto collapsed = driver.collapseInputs(target);
    ASSERT_EQ(collapsed.size(), 2);
    ASSERT_TRUE(collapsed[0].isScalar());
    ASSERT_TRUE(collapsed[1].isScalar());
    EXPECT_DOUBLE_EQ(collapsed[0].scalarValue, 5.0);
    EXPECT_DOUBLE_EQ(collapsed[1].scalarValue, 2.0);

    EXPECT_TRUE(
        driver.simulate({UUID(3), target, UUID::null, TimeNs(0)}, collapsed));
    const auto output =
        driver.getPortState(scalarPort(target, PortDirection::output, 0));
    ASSERT_TRUE(output.isScalar());
    EXPECT_DOUBLE_EQ(output.scalarValue, 7.0);
}

TEST(MathSimDriverTest, ConnectionsValidateAndPropagateStringPorts) {
    MathSimDriver driver;
    driver.init();

    const auto stringDefinition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "String Input");
    const auto derivativeDefinition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Diffrentiate");
    ASSERT_NE(stringDefinition, nullptr);
    ASSERT_NE(derivativeDefinition, nullptr);

    const auto stringId = addMathComponent(driver, stringDefinition);
    const auto derivativeId = addMathComponent(driver, derivativeDefinition);
    ASSERT_NE(stringId, UUID::null);
    ASSERT_NE(derivativeId, UUID::null);

    EXPECT_FALSE(
        driver
            .canConnectPorts(stringPort(stringId, PortDirection::output, 0),
                             scalarPort(derivativeId, PortDirection::input, 0))
            .first);
    ASSERT_TRUE(
        driver.connectPorts(stringPort(stringId, PortDirection::output, 0),
                            stringPort(derivativeId, PortDirection::input, 1),
                            false));
    ASSERT_TRUE(
        driver.setOutputPortState(stringId, 0, PortState::string("theta")));

    const auto collapsed = driver.collapseInputs(derivativeId);
    ASSERT_EQ(collapsed.size(), 2u);
    ASSERT_TRUE(collapsed[1].isString());
    EXPECT_EQ(collapsed[1].stringValue, "theta");
}

TEST(MathSimDriverTest, VariableNodeBridgesSymbolicNamesAndScalarValues) {
    MathSimDriver driver;
    driver.init();

    const auto definition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Variable");
    ASSERT_NE(definition, nullptr);

    const auto inputs = definition->getInputPortDescriptor();
    const auto outputs = definition->getOutputPortDescriptor();
    ASSERT_EQ(inputs.portCount(), 2u);
    EXPECT_EQ(inputs.nameAt(0), "Name");
    EXPECT_EQ(inputs.signalKindAt(0), SignalKind::string);
    EXPECT_EQ(inputs.nameAt(1), "Value");
    EXPECT_EQ(inputs.signalKindAt(1), SignalKind::scalar);
    EXPECT_EQ(inputs.makeInitialStates()[0].stringValue, "x");
    ASSERT_EQ(outputs.portCount(), 1u);
    EXPECT_EQ(outputs.signalKindAt(0), SignalKind::scalar);

    const auto variableId = addMathComponent(driver, definition);
    ASSERT_NE(variableId, UUID::null);
    ASSERT_TRUE(
        driver.setInputPortState(variableId, 1, PortState::scalar(3.5)));

    auto collapsed = driver.collapseInputs(variableId);
    EXPECT_TRUE(driver.simulate({UUID(20), variableId, UUID::null, TimeNs(0)},
                                collapsed));

    const auto output =
        driver.getPortState(scalarPort(variableId, PortDirection::output, 0));
    ASSERT_TRUE(output.isScalar());
    EXPECT_DOUBLE_EQ(output.scalarValue, 3.5);

    auto component = driver.getComponent<MathSimComp>(variableId);
    ASSERT_NE(component, nullptr);
    auto symbolicDefinition = component->getFnDef();
    EXPECT_EQ(symbolicDefinition.toString(), "x");
    ASSERT_TRUE(symbolicDefinition.variableValues.contains("x"));
    EXPECT_DOUBLE_EQ(symbolicDefinition.variableValues.at("x"), 3.5);
    EXPECT_DOUBLE_EQ(symbolicDefinition.evaluate(), 3.5);

    ASSERT_TRUE(driver.setInputPortState(
        variableId, 0, PortState::string("  theta  ")));
    collapsed = driver.collapseInputs(variableId);
    EXPECT_TRUE(driver.simulate({UUID(21), variableId, UUID::null, TimeNs(0)},
                                collapsed));

    symbolicDefinition = component->getFnDef();
    EXPECT_EQ(symbolicDefinition.toString(), "theta");
    EXPECT_TRUE(symbolicDefinition.variableValues.contains("theta"));
    EXPECT_EQ(component->getInputStates()[0].stringValue, "theta");

    ASSERT_TRUE(
        driver.setInputPortState(variableId, 0, PortState::string("t")));
    collapsed = driver.collapseInputs(variableId);
    EXPECT_TRUE(driver.simulate({UUID(22), variableId, UUID::null, TimeNs(0)},
                                collapsed));
    EXPECT_EQ(component->getFnDef().toString(), "x");
    EXPECT_EQ(component->getInputStates()[0].stringValue, "x");

    ASSERT_TRUE(
        driver.setInputPortState(variableId, 0, PortState::string("2invalid")));
    collapsed = driver.collapseInputs(variableId);
    EXPECT_FALSE(driver.simulate({UUID(23), variableId, UUID::null, TimeNs(0)},
                                 collapsed));
    EXPECT_EQ(component->getFnDef().toString(), "x");
    EXPECT_EQ(component->getInputStates()[0].stringValue, "x");
}

TEST(MathSimDriverTest, SymbolicEvaluationRejectsAmbiguousVariableValues) {
    const auto x = symcalc::Equation("x");

    FnDef left{x};
    left.variableValues["x"] = 2.0;
    FnDef right{x};
    right.variableValues["x"] = 3.0;

    FnDef product{x * x};
    product.mergeVariableValues(left);
    product.mergeVariableValues(right);
    EXPECT_TRUE(product.conflictingVariables.contains("x"));
    EXPECT_TRUE(std::isnan(product.evaluate()));

    FnDef consistentProduct{x * x};
    right.variableValues["x"] = 2.0;
    consistentProduct.mergeVariableValues(left);
    consistentProduct.mergeVariableValues(right);
    EXPECT_TRUE(consistentProduct.conflictingVariables.empty());
    EXPECT_DOUBLE_EQ(consistentProduct.evaluate(), 4.0);
}

TEST(MathSimDriverTest,
     DifferentiateUsesTypedVariableInputAndCurrentSymbolicDefinition) {
    MathSimDriver driver;
    driver.init();

    const auto timeDefinition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Time Node");
    const auto derivativeDefinition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Diffrentiate");
    ASSERT_NE(timeDefinition, nullptr);
    ASSERT_NE(derivativeDefinition, nullptr);

    const auto derivativeInputs =
        derivativeDefinition->getInputPortDescriptor();
    ASSERT_EQ(derivativeInputs.portCount(), 2u);
    EXPECT_EQ(derivativeInputs.signalKindAt(0), SignalKind::scalar);
    EXPECT_EQ(derivativeInputs.signalKindAt(1), SignalKind::string);
    EXPECT_EQ(derivativeInputs.makeInitialStates()[1].stringValue, "t");

    const auto timeId = addMathComponent(driver, timeDefinition);
    const auto secondsDerivativeId =
        addMathComponent(driver, derivativeDefinition);
    const auto millisecondsDerivativeId =
        addMathComponent(driver, derivativeDefinition);
    ASSERT_NE(timeId, UUID::null);
    ASSERT_NE(secondsDerivativeId, UUID::null);
    ASSERT_NE(millisecondsDerivativeId, UUID::null);

    ASSERT_TRUE(driver.connectPorts(
        scalarPort(timeId, PortDirection::output, 0),
        scalarPort(secondsDerivativeId, PortDirection::input, 0),
        false));
    ASSERT_TRUE(driver.connectPorts(
        scalarPort(timeId, PortDirection::output, 1),
        scalarPort(millisecondsDerivativeId, PortDirection::input, 0),
        false));

    EXPECT_FALSE(driver.setInputPortState(
        secondsDerivativeId, 1, PortState::scalar(1.0)));
    ASSERT_TRUE(driver.setInputPortState(
        secondsDerivativeId, 1, PortState::string("x")));
    auto inputs = driver.collapseInputs(secondsDerivativeId);
    driver.simulate({UUID(10), secondsDerivativeId, UUID::null, TimeNs(0)},
                    inputs);
    EXPECT_DOUBLE_EQ(driver
                         .getPortState(scalarPort(
                             secondsDerivativeId, PortDirection::output, 0))
                         .scalarValue,
                     0.0);

    ASSERT_TRUE(driver.setInputPortState(
        secondsDerivativeId, 1, PortState::string("t")));
    inputs = driver.collapseInputs(secondsDerivativeId);
    driver.simulate({UUID(11), secondsDerivativeId, UUID::null, TimeNs(0)},
                    inputs);
    EXPECT_DOUBLE_EQ(driver
                         .getPortState(scalarPort(
                             secondsDerivativeId, PortDirection::output, 0))
                         .scalarValue,
                     1.0);

    const auto millisecondsInputs =
        driver.collapseInputs(millisecondsDerivativeId);
    driver.simulate({UUID(12), millisecondsDerivativeId, UUID::null, TimeNs(0)},
                    millisecondsInputs);
    EXPECT_DOUBLE_EQ(
        driver
            .getPortState(
                scalarPort(millisecondsDerivativeId, PortDirection::output, 0))
            .scalarValue,
        1000.0);

    const auto variableState = driver.getPortState(
        stringPort(secondsDerivativeId, PortDirection::input, 1));
    ASSERT_TRUE(variableState.isString());
    EXPECT_EQ(variableState.stringValue, "t");
}

TEST(MathSimDriverTest,
     VariableExpressionsEvaluateAndDifferentiateAtCurrentValues) {
    MathSimDriver driver;
    driver.init();

    auto definition = [&](const std::string &name) {
        return ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            name);
    };
    const auto variableDef = definition("Variable");
    const auto powerDef = definition("Power (a^b)");
    const auto multiplyDef = definition("Multiply");
    const auto addDef = definition("Add");
    const auto derivativeDef = definition("Diffrentiate");
    ASSERT_NE(variableDef, nullptr);
    ASSERT_NE(powerDef, nullptr);
    ASSERT_NE(multiplyDef, nullptr);
    ASSERT_NE(addDef, nullptr);
    ASSERT_NE(derivativeDef, nullptr);

    const auto variableId = addMathComponent(driver, variableDef);
    const auto powerId = addMathComponent(driver, powerDef);
    const auto multiplyId = addMathComponent(driver, multiplyDef);
    const auto termsId = addMathComponent(driver, addDef);
    const auto polynomialId = addMathComponent(driver, addDef);
    const auto derivativeId = addMathComponent(driver, derivativeDef);
    ASSERT_NE(variableId, UUID::null);
    ASSERT_NE(powerId, UUID::null);
    ASSERT_NE(multiplyId, UUID::null);
    ASSERT_NE(termsId, UUID::null);
    ASSERT_NE(polynomialId, UUID::null);
    ASSERT_NE(derivativeId, UUID::null);

    ASSERT_TRUE(
        driver.setInputPortState(variableId, 0, PortState::string("x")));
    ASSERT_TRUE(
        driver.setInputPortState(variableId, 1, PortState::scalar(3.0)));
    ASSERT_TRUE(driver.setInputPortState(powerId, 1, PortState::scalar(2.0)));
    ASSERT_TRUE(
        driver.setInputPortState(multiplyId, 1, PortState::scalar(2.0)));
    ASSERT_TRUE(
        driver.setInputPortState(polynomialId, 1, PortState::scalar(4.0)));
    ASSERT_TRUE(
        driver.setInputPortState(derivativeId, 1, PortState::string("x")));

    ASSERT_TRUE(
        driver.connectPorts(scalarPort(variableId, PortDirection::output, 0),
                            scalarPort(powerId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(variableId, PortDirection::output, 0),
                            scalarPort(multiplyId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(powerId, PortDirection::output, 0),
                            scalarPort(termsId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(multiplyId, PortDirection::output, 0),
                            scalarPort(termsId, PortDirection::input, 1),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(termsId, PortDirection::output, 0),
                            scalarPort(polynomialId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(polynomialId, PortDirection::output, 0),
                            scalarPort(derivativeId, PortDirection::input, 0),
                            false));

    uint64_t eventId = 30;
    const auto simulate = [&](const UUID &id) {
        return driver.simulate({UUID(eventId++), id, UUID::null, TimeNs(0)},
                               driver.collapseInputs(id));
    };
    ASSERT_TRUE(simulate(variableId));
    ASSERT_TRUE(simulate(powerId));
    ASSERT_TRUE(simulate(multiplyId));
    ASSERT_TRUE(simulate(termsId));
    ASSERT_TRUE(simulate(polynomialId));
    ASSERT_TRUE(simulate(derivativeId));

    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(polynomialId, PortDirection::output, 0))
            .scalarValue,
        19.0);
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(derivativeId, PortDirection::output, 0))
            .scalarValue,
        8.0);

    const auto polynomialDefinition =
        driver.getComponent<MathSimComp>(polynomialId)->getFnDef();
    EXPECT_DOUBLE_EQ(polynomialDefinition.evaluate(), 19.0);
    ASSERT_TRUE(polynomialDefinition.variableValues.contains("x"));
    EXPECT_DOUBLE_EQ(polynomialDefinition.variableValues.at("x"), 3.0);

    ASSERT_TRUE(
        driver.setInputPortState(variableId, 1, PortState::scalar(4.0)));
    EXPECT_TRUE(simulate(variableId));
    EXPECT_TRUE(simulate(powerId));
    EXPECT_TRUE(simulate(multiplyId));
    EXPECT_TRUE(simulate(termsId));
    EXPECT_TRUE(simulate(polynomialId));
    EXPECT_TRUE(simulate(derivativeId));
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(polynomialId, PortDirection::output, 0))
            .scalarValue,
        28.0);
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(derivativeId, PortDirection::output, 0))
            .scalarValue,
        10.0);

    ASSERT_TRUE(
        driver.setInputPortState(variableId, 0, PortState::string("y")));
    EXPECT_TRUE(simulate(variableId));
    EXPECT_TRUE(simulate(powerId));
    EXPECT_TRUE(simulate(multiplyId));
    EXPECT_TRUE(simulate(termsId));
    EXPECT_TRUE(simulate(polynomialId));
    EXPECT_TRUE(simulate(derivativeId));
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(derivativeId, PortDirection::output, 0))
            .scalarValue,
        0.0);

    ASSERT_TRUE(
        driver.setInputPortState(derivativeId, 1, PortState::string("y")));
    EXPECT_TRUE(simulate(derivativeId));
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(derivativeId, PortDirection::output, 0))
            .scalarValue,
        10.0);

    MathSimDriver restoredDriver;
    restoredDriver.init();
    restoredDriver.loadJson(driver.toJson());

    const auto restoredPolynomial =
        restoredDriver.getComponent<MathSimComp>(polynomialId);
    const auto restoredDerivative =
        restoredDriver.getComponent<MathSimComp>(derivativeId);
    ASSERT_NE(restoredPolynomial, nullptr);
    ASSERT_NE(restoredDerivative, nullptr);
    EXPECT_DOUBLE_EQ(restoredPolynomial->getFnDef().evaluate(), 28.0);
    EXPECT_DOUBLE_EQ(restoredDerivative->getFnDef().evaluate(), 10.0);
    EXPECT_TRUE(restoredPolynomial->getFnDef().variableValues.contains("y"));
}

TEST(MathSimDriverTest,
     IntegratorUsesElapsedTimeSupportsResetAndRestartsCleanly) {
    MathSimDriver driver;
    driver.init();

    const auto definition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Integrator");
    ASSERT_NE(definition, nullptr);

    const auto inputs = definition->getInputPortDescriptor();
    const auto outputs = definition->getOutputPortDescriptor();
    ASSERT_EQ(inputs.portCount(), 3u);
    EXPECT_EQ(inputs.nameAt(0), "X");
    EXPECT_EQ(inputs.nameAt(1), "Initial Value");
    EXPECT_EQ(inputs.nameAt(2), "Reset");
    EXPECT_EQ(inputs.signalKindAt(0), SignalKind::scalar);
    EXPECT_EQ(inputs.signalKindAt(1), SignalKind::scalar);
    EXPECT_EQ(inputs.signalKindAt(2), SignalKind::scalar);
    ASSERT_EQ(outputs.portCount(), 1u);
    EXPECT_EQ(outputs.nameAt(0), "Y");

    const auto integratorId = addMathComponent(driver, definition, true);
    ASSERT_NE(integratorId, UUID::null);
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 0, PortState::scalar(2.0)));
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 1, PortState::scalar(1.0)));

    ASSERT_TRUE(driver.beginRun(TimeNs(0)));
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.0);

    EXPECT_EQ(driver.processEventsAt(TimeNs(0)), 1u);
    EXPECT_EQ(driver.processEventsAt(TimeNs(1e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.002,
        1e-12);

    // An off-grid input event updates the integral once, but must not create
    // another periodic timer chain alongside the existing 1 ms chain.
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 0, PortState::scalar(4.0)));
    EXPECT_EQ(driver.processEventsAt(TimeNs(1.5e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.0035,
        1e-12);
    ASSERT_TRUE(driver.getNextEventTime().has_value());
    EXPECT_EQ(*driver.getNextEventTime(), TimeNs(2e6));

    EXPECT_EQ(driver.processEventsAt(TimeNs(2e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.0055,
        1e-12);
    ASSERT_TRUE(driver.getNextEventTime().has_value());
    EXPECT_EQ(*driver.getNextEventTime(), TimeNs(3e6));

    // Re-evaluating at an already-integrated timestamp must not add area.
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 0, PortState::scalar(6.0)));
    EXPECT_EQ(driver.processEventsAt(TimeNs(2e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.0055,
        1e-12);
    EXPECT_EQ(driver.processEventsAt(TimeNs(3e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        1.0115,
        1e-12);

    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 1, PortState::scalar(5.0)));
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 2, PortState::scalar(1.0)));
    EXPECT_EQ(driver.processEventsAt(TimeNs(3e6)), 1u);
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        5.0);
    EXPECT_EQ(driver.processEventsAt(TimeNs(4e6)), 1u);
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        5.0);

    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 2, PortState::scalar(0.0)));
    EXPECT_EQ(driver.processEventsAt(TimeNs(4e6)), 1u);
    EXPECT_EQ(driver.processEventsAt(TimeNs(5e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        5.006,
        1e-12);

    driver.stop();
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 1, PortState::scalar(3.0)));
    constexpr auto restartTime = TimeNs(10e9);
    ASSERT_TRUE(driver.beginRun(restartTime));
    EXPECT_DOUBLE_EQ(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        3.0);
    EXPECT_EQ(driver.processEventsAt(restartTime), 1u);
    EXPECT_EQ(driver.processEventsAt(restartTime + TimeNs(1e6)), 1u);
    EXPECT_NEAR(
        driver.getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        3.002,
        1e-12);
    driver.stop();
}

TEST(MathSimDriverTest, IntegratorPreservesConfiguredInitialValueOnSaveLoad) {
    MathSimDriver driver;
    driver.init();

    const auto definition =
        ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            "Integrator");
    ASSERT_NE(definition, nullptr);
    const auto integratorId = addMathComponent(driver, definition, true);
    ASSERT_NE(integratorId, UUID::null);
    ASSERT_TRUE(
        driver.setInputPortState(integratorId, 1, PortState::scalar(-2.5)));

    MathSimDriver restored;
    restored.init();
    restored.loadJson(driver.toJson());
    ASSERT_TRUE(restored.beginRun(TimeNs(0)));

    const auto restoredIntegrator =
        restored.getComponent<MathSimComp>(integratorId);
    ASSERT_NE(restoredIntegrator, nullptr);
    EXPECT_DOUBLE_EQ(
        restored
            .getPortState(scalarPort(integratorId, PortDirection::output, 0))
            .scalarValue,
        -2.5);
    EXPECT_FALSE(restoredIntegrator->getRuntimeState().initialized);
    EXPECT_EQ(restored.processEventsAt(TimeNs(0)), 1u);
    EXPECT_TRUE(restoredIntegrator->getRuntimeState().initialized);
    EXPECT_DOUBLE_EQ(restoredIntegrator->getRuntimeState().values.at(0), -2.5);
    restored.stop();
}

TEST(MathSimDriverTest, IntegratorsSimulateADampedSpringMassSystem) {
    MathSimDriver driver;
    driver.init();

    const auto getDefinition = [](const std::string &name) {
        return ComponentCatalog::instance().getComponentDefinition<MathCompDef>(
            name);
    };
    const auto integratorDef = getDefinition("Integrator");
    const auto multiplyDef = getDefinition("Multiply");
    const auto addDef = getDefinition("Add");
    ASSERT_NE(integratorDef, nullptr);
    ASSERT_NE(multiplyDef, nullptr);
    ASSERT_NE(addDef, nullptr);

    const auto positionId = addMathComponent(driver, integratorDef, true);
    const auto velocityId = addMathComponent(driver, integratorDef, true);
    const auto springForceId = addMathComponent(driver, multiplyDef, true);
    const auto dampingForceId = addMathComponent(driver, multiplyDef, true);
    const auto accelerationId = addMathComponent(driver, addDef, true);
    ASSERT_NE(positionId, UUID::null);
    ASSERT_NE(velocityId, UUID::null);
    ASSERT_NE(springForceId, UUID::null);
    ASSERT_NE(dampingForceId, UUID::null);
    ASSERT_NE(accelerationId, UUID::null);

    // m = 1 kg, k = 100 N/m, c = 2 N*s/m, x(0) = 0.1 m.
    ASSERT_TRUE(
        driver.setInputPortState(positionId, 1, PortState::scalar(0.1)));
    ASSERT_TRUE(
        driver.setInputPortState(velocityId, 1, PortState::scalar(0.0)));
    ASSERT_TRUE(
        driver.setInputPortState(springForceId, 1, PortState::scalar(-100.0)));
    ASSERT_TRUE(
        driver.setInputPortState(dampingForceId, 1, PortState::scalar(-2.0)));

    ASSERT_TRUE(
        driver.connectPorts(scalarPort(positionId, PortDirection::output, 0),
                            scalarPort(springForceId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(velocityId, PortDirection::output, 0),
                            scalarPort(dampingForceId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(springForceId, PortDirection::output, 0),
                            scalarPort(accelerationId, PortDirection::input, 0),
                            false));
    ASSERT_TRUE(driver.connectPorts(
        scalarPort(dampingForceId, PortDirection::output, 0),
        scalarPort(accelerationId, PortDirection::input, 1),
        false));
    ASSERT_TRUE(driver.connectPorts(
        scalarPort(accelerationId, PortDirection::output, 0),
        scalarPort(velocityId, PortDirection::input, 0),
        false));
    ASSERT_TRUE(
        driver.connectPorts(scalarPort(velocityId, PortDirection::output, 0),
                            scalarPort(positionId, PortDirection::input, 0),
                            false));

    ASSERT_TRUE(driver.beginRun(TimeNs(0)));
    constexpr auto endTime = TimeNs(2e9);
    size_t processedBatches = 0;
    bool sawNegativeDisplacement = false;
    double latePeakDisplacement = 0.0;

    while (const auto nextTime = driver.getNextEventTime()) {
        if (*nextTime > endTime) {
            break;
        }
        ASSERT_LT(processedBatches++, 50000u);
        ASSERT_GT(driver.processEventsAt(*nextTime), 0u);

        const double displacement =
            driver
                .getPortState(scalarPort(positionId, PortDirection::output, 0))
                .scalarValue;
        ASSERT_TRUE(std::isfinite(displacement));
        sawNegativeDisplacement =
            sawNegativeDisplacement || displacement < -0.01;
        if (*nextTime >= TimeNs(1.4e9)) {
            latePeakDisplacement =
                std::max(latePeakDisplacement, std::abs(displacement));
        }
    }

    EXPECT_TRUE(sawNegativeDisplacement);
    EXPECT_GT(latePeakDisplacement, 0.0);
    EXPECT_LT(latePeakDisplacement, 0.04);
    EXPECT_LT(processedBatches, 30000u);
    driver.stop();
}

TEST(MathSimDriverTest, RejectsNonScalarPortsAndStates) {
    MathSimDriver driver;
    driver.init();

    const auto componentId = addMathComponent(
        driver, MathCompDef::makeBinaryOp("Add Test", "Math", MathOpKind::add));
    ASSERT_NE(componentId, UUID::null);

    EXPECT_FALSE(driver.setInputPortState(
        componentId, 0, PortState::digital(LogicState::high)));
    EXPECT_FALSE(driver.setOutputPortState(
        componentId, 0, PortState::digital(LogicState::high)));

    const auto [canConnect, error] = driver.canConnectPorts(
        digitalPort(componentId, PortDirection::output, 0),
        scalarPort(componentId, PortDirection::input, 0));
    EXPECT_FALSE(canConnect);
    EXPECT_FALSE(error.empty());
}

TEST(MathSimDriverTest, RunClearsPendingEventsBeforeStartup) {
    MathSimDriver driver;
    driver.init();

    std::atomic<int> staleRuns{0};
    std::atomic<int> selfRuns{0};

    const auto staleDef = makeMathTestDef(
        "Stale Event Target",
        0,
        1,
        [&staleRuns](const std::shared_ptr<MathCompSimData> &data) {
            staleRuns.fetch_add(1);
            return data;
        });

    const auto staleId = addMathComponent(driver, staleDef, false);
    ASSERT_NE(staleId, UUID::null);
    driver.scheduleEvt(staleId, TimeNs(0), UUID::master, false);

    const auto selfDef = makeMathTestDef(
        "Self Scheduler",
        0,
        1,
        [&driver, &selfRuns](const std::shared_ptr<MathCompSimData> &data) {
            selfRuns.fetch_add(1);
            data->outputStates = {PortState::scalar(1.0, data->simTime)};
            driver.stop();
            return data;
        },
        true);

    ASSERT_NE(addMathComponent(driver, selfDef, true), UUID::null);

    ASSERT_TRUE(
        runDriverUntil(driver, [&selfRuns]() { return selfRuns.load() > 0; }));

    EXPECT_EQ(selfRuns.load(), 1);
    EXPECT_EQ(staleRuns.load(), 0);
}

TEST(MathSimDriverTest, RunPreservesInitialSchedulingWhenNoSelfSchedulers) {
    MathSimDriver driver;
    driver.init();

    std::atomic<int> runs{0};

    const auto definition = makeMathTestDef(
        "Initial Event Target",
        0,
        1,
        [&driver, &runs](const std::shared_ptr<MathCompSimData> &data) {
            runs.fetch_add(1);
            driver.stop();
            return data;
        });

    ASSERT_NE(addMathComponent(driver, definition, true), UUID::null);

    ASSERT_TRUE(runDriverUntil(driver, [&runs]() { return runs.load() > 0; }));
    EXPECT_EQ(runs.load(), 1);
}

TEST(MathSimDriverTest, RunStartsSelfSchedulersBeforeInitialDependants) {
    MathSimDriver driver;
    driver.init();

    std::atomic<int> targetRuns{0};
    std::atomic<int> independentRuns{0};
    std::atomic<bool> targetSawUpdatedSource{false};
    std::atomic<bool> targetSawStaleSource{false};

    const auto sourceDef = makeMathTestDef(
        "Self Scheduled Source",
        0,
        1,
        [](const std::shared_ptr<MathCompSimData> &data) {
            data->outputStates = {PortState::scalar(42.0, data->simTime)};
            data->simDependants = true;
            return data;
        },
        true);

    const auto targetDef = makeMathTestDef(
        "Dependent Target",
        1,
        1,
        [&driver, &targetRuns, &targetSawUpdatedSource, &targetSawStaleSource](
            const std::shared_ptr<MathCompSimData> &data) {
            targetRuns.fetch_add(1);

            const double input = data->inputStates.empty()
                                     ? 0.0
                                     : data->inputStates[0].scalarValue;
            if (input == 42.0) {
                targetSawUpdatedSource.store(true);
                driver.stop();
            } else {
                targetSawStaleSource.store(true);
            }

            return data;
        });

    const auto independentDef = makeMathTestDef(
        "Independent Initial Target",
        0,
        1,
        [&independentRuns](const std::shared_ptr<MathCompSimData> &data) {
            independentRuns.fetch_add(1);
            return data;
        });

    const auto sourceId = addMathComponent(driver, sourceDef, true);
    const auto targetId = addMathComponent(driver, targetDef, true);
    const auto independentId = addMathComponent(driver, independentDef, true);
    ASSERT_NE(sourceId, UUID::null);
    ASSERT_NE(targetId, UUID::null);
    ASSERT_NE(independentId, UUID::null);

    ASSERT_TRUE(
        driver.connectPorts(scalarPort(sourceId, PortDirection::output, 0),
                            scalarPort(targetId, PortDirection::input, 0),
                            false));

    ASSERT_TRUE(runDriverUntil(driver, [&targetSawUpdatedSource]() {
        return targetSawUpdatedSource.load();
    }));

    EXPECT_EQ(targetRuns.load(), 1);
    EXPECT_EQ(independentRuns.load(), 1);
    EXPECT_TRUE(targetSawUpdatedSource.load());
    EXPECT_FALSE(targetSawStaleSource.load());
}
