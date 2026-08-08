#include "common/types.h"
#include "dig_sim_driver.h"
#include "math_sim_driver.h"
#include "simulation_engine.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
    using Bess::TimeMs;
    using Bess::TimeNs;
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers;
    using namespace Bess::SimEngine::Drivers::Digital;
    using namespace Bess::SimEngine::Drivers::Math;

    bool waitUntilStopped(
        SimulationEngine &engine,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (engine.getSimulationState() != SimulationState::stopped &&
               std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return engine.getSimulationState() == SimulationState::stopped;
    }

    std::shared_ptr<MathCompDef>
    makeClockedDefinition(const std::string &name,
                          TimeNs period,
                          const MathCompDef::TMathSimFn &simFn) {
        auto definition = std::make_shared<MathCompDef>();
        definition->setName(name);
        definition->setGroupName("Timing Test");
        definition->setInputPortDescriptor({
            .direction = PortDirection::input,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = 0,
        });
        definition->setOutputPortDescriptor({
            .direction = PortDirection::output,
            .signalKind = SignalKind::scalar,
            .quantityKind = QuantityKind::dimensionless,
            .count = 1,
        });
        definition->setSimFn(simFn);
        definition->setAutoReschedule(true);
        definition->setAutoRescheduleDelay(period);
        return definition;
    }
} // namespace

class SimulationTimingTest : public testing::Test {
  protected:
    void SetUp() override {
        engine = std::make_unique<SimulationEngine>();
        engine->onInit();
    }

    void TearDown() override {
        if (engine) {
            engine->stop();
            engine->destroy();
        }
    }

    std::unique_ptr<SimulationEngine> engine;
};

TEST_F(SimulationTimingTest, TimedRunUsesExactGlobalTimeAndSampleBoundaries) {
    std::vector<double> simulatedAtNs;
    const auto definition = makeClockedDefinition(
        "Exact Global Clock",
        TimeNs(1e6),
        [&simulatedAtNs](const std::shared_ptr<MathCompSimData> &data) {
            simulatedAtNs.push_back(data->simTime.count());
            data->outputStates[0] =
                PortState::scalar(data->simTime.count(), data->simTime);
            return data;
        });

    const auto componentId = engine->addComponent(definition, false);
    ASSERT_NE(componentId, Bess::UUID::null);

    engine->runFor(TimeMs(5), TimeMs(2));
    ASSERT_TRUE(waitUntilStopped(*engine));
    engine->stop();

    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 5e6);
    const auto runContext = engine->getRunCtx();
    EXPECT_DOUBLE_EQ(runContext.elapsedTime.count(), 5.0);

    const std::vector<double> expectedEventTimes = {
        0.0, 1e6, 2e6, 3e6, 4e6, 5e6};
    EXPECT_EQ(simulatedAtNs, expectedEventTimes);

    for (const auto &driver : engine->getDrivers()) {
        EXPECT_EQ(driver->getSimulationClock(),
                  engine->getDrivers().front()->getSimulationClock());
        EXPECT_DOUBLE_EQ(driver->getCurrentSimTime().count(), 5e6);
    }

    const auto stampData = engine->getStampData();
    const auto stampHistory = stampData.find(componentId);
    ASSERT_TRUE(stampHistory.has_value());
    ASSERT_EQ(stampHistory->samples.size(), 4u);

    const std::vector<double> expectedSampleTimes = {0.0, 2e6, 4e6, 5e6};
    for (size_t i = 0; i < expectedSampleTimes.size(); ++i) {
        EXPECT_DOUBLE_EQ(stampHistory->samples[i].simTime.count(),
                         expectedSampleTimes[i]);
        ASSERT_EQ(stampHistory->samples[i].outputStates.size(), 1u);
        EXPECT_DOUBLE_EQ(stampHistory->samples[i].outputStates[0].scalarValue,
                         expectedSampleTimes[i]);
    }
}

TEST_F(SimulationTimingTest, NormalRunStopInterruptsFarFutureEventWait) {
    std::atomic<int> simulationCount{0};
    const auto definition = makeClockedDefinition(
        "Interruptible Clock",
        TimeNs(5e9),
        [&simulationCount](const std::shared_ptr<MathCompSimData> &data) {
            simulationCount.fetch_add(1);
            data->outputStates[0] = PortState::scalar(1.0, data->simTime);
            return data;
        });

    ASSERT_NE(engine->addComponent(definition, false), Bess::UUID::null);
    engine->run();

    const auto initialDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (simulationCount.load() == 0 &&
           std::chrono::steady_clock::now() < initialDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(simulationCount.load(), 1);

    const auto stopStart = std::chrono::steady_clock::now();
    engine->stop();
    const auto stopDuration = std::chrono::steady_clock::now() - stopStart;

    EXPECT_LT(stopDuration, std::chrono::milliseconds(100));
    EXPECT_EQ(engine->getSimulationState(), SimulationState::stopped);
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 0.0);
}

TEST_F(SimulationTimingTest, PausingFreezesTheGlobalVirtualClock) {
    std::atomic<int> simulationCount{0};
    const auto definition = makeClockedDefinition(
        "Pausable Clock",
        TimeNs(20e6),
        [&simulationCount](const std::shared_ptr<MathCompSimData> &data) {
            simulationCount.fetch_add(1);
            data->outputStates[0] =
                PortState::scalar(simulationCount.load(), data->simTime);
            return data;
        });

    ASSERT_NE(engine->addComponent(definition, false), Bess::UUID::null);
    engine->run();

    const auto initialDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (simulationCount.load() == 0 &&
           std::chrono::steady_clock::now() < initialDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(simulationCount.load(), 1);

    engine->setSimulationState(SimulationState::paused);
    const auto pausedTime = engine->getCurrentSimTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(40));

    EXPECT_EQ(engine->getSimulationState(), SimulationState::paused);
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), pausedTime.count());
    EXPECT_EQ(simulationCount.load(), 1);

    engine->setSimulationState(SimulationState::running);
    const auto resumedDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (simulationCount.load() < 2 &&
           std::chrono::steady_clock::now() < resumedDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_GE(simulationCount.load(), 2);
    engine->stop();
}

TEST_F(SimulationTimingTest, InvalidZeroSelfDelayDoesNotLivelock) {
    std::atomic<int> simulationCount{0};
    const auto definition = makeClockedDefinition(
        "Invalid Zero Clock",
        TimeNs(0),
        [&simulationCount](const std::shared_ptr<MathCompSimData> &data) {
            simulationCount.fetch_add(1);
            data->outputStates[0] = PortState::scalar(1.0, data->simTime);
            return data;
        });

    ASSERT_NE(engine->addComponent(definition, false), Bess::UUID::null);
    engine->runFor(TimeMs(1), TimeMs(0));
    ASSERT_TRUE(waitUntilStopped(*engine));
    engine->stop();

    EXPECT_EQ(simulationCount.load(), 1);
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 1e6);
}

TEST_F(SimulationTimingTest, TimedRunKeepsDigitalAndMathDriversInLockstep) {
    std::vector<double> digitalTimes;
    std::vector<double> mathTimes;

    auto digitalDefinition = std::make_shared<DigCompDef>();
    digitalDefinition->setName("Digital Global Clock");
    digitalDefinition->setGroupName("Timing Test");
    digitalDefinition->setInputSlotsInfo(
        {SlotsGroupType::input, false, 0, {}, {}});
    digitalDefinition->setOutputSlotsInfo(
        {SlotsGroupType::output, false, 1, {}, {}});
    digitalDefinition->setAutoReschedule(true);
    digitalDefinition->setAutoRescheduleDelay(TimeNs(1e6));
    digitalDefinition->setSimFn(
        [&digitalTimes](const std::shared_ptr<DigCompSimData> &data) {
            digitalTimes.push_back(data->simTime.count());
            data->outputStates[0] =
                PortState::digital(LogicState::high, data->simTime);
            return data;
        });

    const auto mathDefinition = makeClockedDefinition(
        "Math Global Clock",
        TimeNs(1.5e6),
        [&mathTimes](const std::shared_ptr<MathCompSimData> &data) {
            mathTimes.push_back(data->simTime.count());
            data->outputStates[0] =
                PortState::scalar(data->simTime.count(), data->simTime);
            return data;
        });

    ASSERT_NE(engine->addComponent(digitalDefinition, false), Bess::UUID::null);
    ASSERT_NE(engine->addComponent(mathDefinition, false), Bess::UUID::null);

    engine->runFor(TimeMs(3), TimeMs(0));
    ASSERT_TRUE(waitUntilStopped(*engine));

    EXPECT_EQ(digitalTimes, (std::vector<double>{0.0, 1e6, 2e6, 3e6}));
    EXPECT_EQ(mathTimes, (std::vector<double>{0.0, 1.5e6, 3e6}));
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 3e6);
}

TEST_F(SimulationTimingTest, PausedTimedRunStepsThroughSamplesAndFinalTime) {
    std::atomic<bool> pauseRequested{false};
    const auto definition = makeClockedDefinition(
        "Step Timed Clock",
        TimeNs(10e6),
        [this, &pauseRequested](const std::shared_ptr<MathCompSimData> &data) {
            data->outputStates[0] =
                PortState::scalar(data->simTime.count(), data->simTime);
            if (!pauseRequested.exchange(true)) {
                engine->setSimulationState(SimulationState::paused);
            }
            return data;
        });

    const auto componentId = engine->addComponent(definition, false);
    ASSERT_NE(componentId, Bess::UUID::null);

    engine->runFor(TimeMs(5), TimeMs(2));
    const auto pauseDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (engine->getSimulationState() != SimulationState::paused &&
           std::chrono::steady_clock::now() < pauseDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(engine->getSimulationState(), SimulationState::paused);

    engine->stepSimulation();
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 2e6);
    engine->stepSimulation();
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 4e6);
    engine->stepSimulation();
    EXPECT_EQ(engine->getSimulationState(), SimulationState::stopped);
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 5e6);

    const auto stamps = engine->getStampData();
    const auto stampHistory = stamps.find(componentId);
    ASSERT_TRUE(stampHistory.has_value());
    ASSERT_EQ(stampHistory->samples.size(), 4u);
    EXPECT_DOUBLE_EQ(stampHistory->samples[0].simTime.count(), 0.0);
    EXPECT_DOUBLE_EQ(stampHistory->samples[1].simTime.count(), 2e6);
    EXPECT_DOUBLE_EQ(stampHistory->samples[2].simTime.count(), 4e6);
    EXPECT_DOUBLE_EQ(stampHistory->samples[3].simTime.count(), 5e6);
}

TEST_F(SimulationTimingTest, DriverCallbackCanStopAPausedStepWithoutDeadlock) {
    std::atomic<int> simulationCount{0};
    const auto definition = makeClockedDefinition(
        "Stopping Step Clock",
        TimeNs(1e6),
        [this, &simulationCount](const std::shared_ptr<MathCompSimData> &data) {
            const auto count = simulationCount.fetch_add(1) + 1;
            data->outputStates[0] = PortState::scalar(count, data->simTime);
            if (count == 1) {
                engine->setSimulationState(SimulationState::paused);
            } else if (count == 2) {
                engine->stop();
            }
            return data;
        });

    ASSERT_NE(engine->addComponent(definition, false), Bess::UUID::null);
    engine->runFor(TimeMs(5));

    const auto pauseDeadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    while (engine->getSimulationState() != SimulationState::paused &&
           std::chrono::steady_clock::now() < pauseDeadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_EQ(engine->getSimulationState(), SimulationState::paused);

    engine->stepSimulation();

    EXPECT_EQ(simulationCount.load(), 2);
    EXPECT_EQ(engine->getSimulationState(), SimulationState::stopped);
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 1e6);
}

TEST_F(SimulationTimingTest, DriverExceptionsStopTheRunWithoutEscapingThread) {
    const auto definition = makeClockedDefinition(
        "Throwing Clock",
        TimeNs(1e6),
        [](const std::shared_ptr<MathCompSimData> &)
            -> std::shared_ptr<MathCompSimData> {
            throw std::runtime_error("intentional timing-test failure");
        });

    ASSERT_NE(engine->addComponent(definition, false), Bess::UUID::null);
    engine->runFor(TimeMs(5), TimeMs(1));

    ASSERT_TRUE(waitUntilStopped(*engine));
    EXPECT_DOUBLE_EQ(engine->getCurrentSimTime().count(), 0.0);
}

TEST_F(SimulationTimingTest,
       TransitionStampsPreserveConnectionChangesAndCompressNaNs) {
    const auto definition = makeClockedDefinition(
        "Stamp Semantics",
        TimeNs(1e6),
        [](const std::shared_ptr<MathCompSimData> &data) { return data; });
    definition->setAutoReschedule(false);

    const auto componentId = engine->addComponent(definition, false);
    ASSERT_NE(componentId, Bess::UUID::null);

    const auto driver = engine->getDriverWithName(MathSimDriver::NAME);
    ASSERT_NE(driver, nullptr);
    driver->clearStampData();
    driver->stampSim(TimeNs(0), false);

    auto disconnected = PortState::scalar(0.0);
    disconnected.connState = ConnectionState::high_z;
    engine->setOutputPortState(componentId, 0, disconnected);
    driver->stampSim(TimeNs(1), false);
    driver->stampSim(TimeNs(2), false);

    auto notANumber =
        PortState::scalar(std::numeric_limits<double>::quiet_NaN());
    notANumber.connState = ConnectionState::high_z;
    engine->setOutputPortState(componentId, 0, notANumber);
    driver->stampSim(TimeNs(3), false);
    driver->stampSim(TimeNs(4), false);

    const auto history = driver->getStampData();
    const auto stampHistory = history.find(componentId);
    ASSERT_TRUE(stampHistory.has_value());
    ASSERT_EQ(stampHistory->samples.size(), 3u);
    EXPECT_EQ(stampHistory->samples[0].outputStates[0].connState,
              ConnectionState::driven);
    EXPECT_EQ(stampHistory->samples[1].outputStates[0].connState,
              ConnectionState::high_z);
    EXPECT_TRUE(
        std::isnan(stampHistory->samples[2].outputStates[0].scalarValue));
}

TEST_F(SimulationTimingTest, EngineStampViewReferencesDriverOwnedHistory) {
    const auto definition = makeClockedDefinition(
        "Referenced Stamp History",
        TimeNs(1e6),
        [](const std::shared_ptr<MathCompSimData> &data) { return data; });
    definition->setAutoReschedule(false);

    const auto componentId = engine->addComponent(definition, false);
    ASSERT_NE(componentId, Bess::UUID::null);

    const auto driver = engine->getDriverWithName(MathSimDriver::NAME);
    ASSERT_NE(driver, nullptr);
    driver->clearStampData();
    driver->stampSim(TimeNs(0), true);

    const SimDriver::ComponentStamp *driverHistoryAddress = nullptr;
    {
        const auto driverData = driver->getStampData();
        const auto driverHistory = driverData.find(componentId);
        ASSERT_TRUE(driverHistory.has_value());
        ASSERT_FALSE(driverHistory->samples.empty());
        driverHistoryAddress = driverHistory->samples.data();
    }

    {
        const auto engineData = engine->getStampData();
        const auto engineHistory = engineData.find(componentId);
        ASSERT_TRUE(engineHistory.has_value());
        EXPECT_EQ(engineHistory->samples.data(), driverHistoryAddress);
    }
}

TEST_F(SimulationTimingTest, StampHistoryIsBoundedForContinuousRuns) {
    const auto definition = makeClockedDefinition(
        "Bounded Stamp History",
        TimeNs(1e6),
        [](const std::shared_ptr<MathCompSimData> &data) { return data; });
    definition->setAutoReschedule(false);

    const auto componentId = engine->addComponent(definition, false);
    ASSERT_NE(componentId, Bess::UUID::null);

    const auto driver = engine->getDriverWithName(MathSimDriver::NAME);
    ASSERT_NE(driver, nullptr);
    driver->clearStampData();

    constexpr std::size_t extraSamples = 128;
    const std::size_t totalSamples =
        SimDriver::MaxStampSamplesPerComponent + extraSamples;
    for (std::size_t i = 0; i < totalSamples; ++i) {
        const auto simTime = TimeNs(static_cast<double>(i));
        engine->setOutputPortState(
            componentId,
            0,
            PortState::scalar(static_cast<double>(i), simTime));
        driver->stampSim(simTime, false);
    }

    const auto stampData = driver->getStampData();
    const auto stampHistory = stampData.find(componentId);
    ASSERT_TRUE(stampHistory.has_value());
    EXPECT_LE(stampHistory->samples.size(),
              SimDriver::MaxStampSamplesPerComponent);
    EXPECT_GT(stampHistory->samples.front().simTime, TimeNs(0));
    EXPECT_DOUBLE_EQ(stampHistory->samples.back().simTime.count(),
                     static_cast<double>(totalSamples - 1U));
}
