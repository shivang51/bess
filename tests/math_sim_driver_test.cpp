#include "common/types.h"
#include "math_sim_driver.h"

#include <atomic>
#include <chrono>
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
        auto definition = MathCompDef::makeBinaryOp(
            "Serialization Test", "Math", kind);

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
