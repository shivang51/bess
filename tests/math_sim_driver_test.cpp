#include "math_sim_driver.h"
#include "common/types.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace {
    using Bess::TimeNs;
    using Bess::UUID;
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers::Math;

    PortRef scalarPort(const UUID &componentId,
                       PortDirection direction,
                       int index) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = SignalKind::scalar,
                .index = index};
    }

    PortRef digitalPort(const UUID &componentId,
                        PortDirection direction,
                        int index) {
        return {.componentId = componentId,
                .direction = direction,
                .signalKind = SignalKind::digital,
                .index = index};
    }

    UUID addMathComponent(MathSimDriver &driver,
                          const std::shared_ptr<MathCompDef> &definition) {
        const auto component =
            std::dynamic_pointer_cast<MathSimComp>(
                driver.createComp(definition, false));
        EXPECT_NE(component, nullptr);
        if (!component) {
            return UUID::null;
        }

        return driver.addComponent(component, false);
    }
} // namespace

TEST(MathSimDriverTest, BuiltInBinaryOpsProduceScalarOutputs) {
    MathSimDriver driver;
    driver.init();

    const auto addId = addMathComponent(
        driver,
        MathCompDef::makeBinaryOp("Add Test", "Math", MathOpKind::add));
    ASSERT_NE(addId, UUID::null);

    const auto subId = addMathComponent(
        driver,
        MathCompDef::makeBinaryOp(
            "Subtract Test", "Math", MathOpKind::subtract));
    ASSERT_NE(subId, UUID::null);

    EXPECT_TRUE(driver.simulate(
        {UUID(1), addId, UUID::null, TimeNs(0)},
        {PortState::scalar(2.5), PortState::scalar(4.0)}));
    auto addOutput = driver.getPortState(
        scalarPort(addId, PortDirection::output, 0));
    EXPECT_TRUE(addOutput.isScalar());
    EXPECT_DOUBLE_EQ(addOutput.scalarValue, 6.5);

    EXPECT_TRUE(driver.simulate(
        {UUID(2), subId, UUID::null, TimeNs(0)},
        {PortState::scalar(7.0), PortState::scalar(2.25)}));
    auto subOutput = driver.getPortState(
        scalarPort(subId, PortDirection::output, 0));
    EXPECT_TRUE(subOutput.isScalar());
    EXPECT_DOUBLE_EQ(subOutput.scalarValue, 4.75);
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

    ASSERT_TRUE(driver.connectPorts(
        scalarPort(sourceA, PortDirection::output, 0),
        scalarPort(target, PortDirection::input, 0),
        false));
    ASSERT_TRUE(driver.connectPorts(
        scalarPort(sourceB, PortDirection::output, 0),
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

TEST(MathSimDriverTest, RejectsNonScalarPortsAndStates) {
    MathSimDriver driver;
    driver.init();

    const auto componentId = addMathComponent(
        driver,
        MathCompDef::makeBinaryOp("Add Test", "Math", MathOpKind::add));
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
