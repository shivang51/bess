#include "common/bess_uuid.h"
#include "drivers/digital_sim_driver.h"
#include "drivers/sim_driver.h"
#include "simulation_engine.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace {
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers;
    using namespace Bess::SimEngine::Drivers::Digital;

    TEST(SimDriverTest, CreateBaseDefinitionStoresTypeName) {
        struct SimData {};

        Digital::DigCompDef definition;

        const auto json = definition.toJson();

        EXPECT_EQ(definition.getTypeName(), DigCompDef::TypeName);
        ASSERT_TRUE(json.isMember("typeName"));
        EXPECT_EQ(json["typeName"].asString(), DigCompDef::TypeName);
    }

    TEST(SimDriverTest, CreateAndGateDefinitionHasExpectedShape) {
        DigCompDef andGate;
        andGate.setName("AND Gate");
        andGate.setGroupName("Logic");
        andGate.setInputSlotsInfo({SlotsGroupType::input, false, 2, {"A", "B"}, {}});
        andGate.setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"Y"}, {}});
        andGate.setOpInfo({'*', false});
        andGate.setPropDelay(Bess::TimeNs(1));
        andGate.setSimFn([](const std::shared_ptr<Drivers::Digital::DigCompSimData> &dataBase) {
            const auto data = std::dynamic_pointer_cast<DigCompSimData>(dataBase);
            if (!data)
                return dataBase;

            data->outputStates.resize(1);
            const bool out = data->inputStates.size() >= 2 &&
                             data->inputStates[0].state == LogicState::high &&
                             data->inputStates[1].state == LogicState::high;
            data->outputStates[0] = SlotState(out ? LogicState::high : LogicState::low,
                                              std::chrono::duration_cast<SimTime>(data->simTime));
            data->simDependants = true;
            return dataBase;
        });

        EXPECT_EQ(andGate.getName(), "AND Gate");
        EXPECT_EQ(andGate.getGroupName(), "Logic");
        EXPECT_EQ(andGate.getInputSlotsInfo().count, 2u);
        EXPECT_EQ(andGate.getOutputSlotsInfo().count, 1u);
        EXPECT_EQ(andGate.getOpInfo().op, '*');
        EXPECT_EQ(andGate.getPropDelay(), Bess::TimeNs(1));
        EXPECT_TRUE(static_cast<bool>(andGate.getSimFn()));

        const auto simFn = andGate.getSimFn();
        auto data = std::make_shared<DigCompSimData>();
        data->inputStates = {SlotState(LogicState::high, Bess::TimeNs(0)), SlotState(LogicState::high, Bess::TimeNs(0))};
        data->simTime = Bess::TimeNs(10);
        const auto nextBase = simFn(data);
        const auto next = std::dynamic_pointer_cast<DigCompSimData>(nextBase);

        ASSERT_NE(next, nullptr);
        ASSERT_EQ(next->outputStates.size(), 1u);
        EXPECT_EQ(next->outputStates[0].state, LogicState::high);
    }

    TEST(SimDriverTest, AddComponentRegistersItInDigitalDriverMap) {
        DigitalSimDriver driver;
        const Bess::UUID compId(0xAA11);

        auto component = std::make_shared<DigSimComp>();
        component->setUuid(compId);
        component->setName("AND#1");

        EXPECT_EQ(driver.getComponent<DigSimComp>(compId), nullptr);

        driver.addComponent(component);

        const auto stored = driver.getComponent<DigSimComp>(compId);
        ASSERT_NE(stored, nullptr);
        EXPECT_EQ(stored->getUuid(), compId);
        EXPECT_EQ(driver.getComponentsMap().size(), 1u);
        EXPECT_TRUE(driver.getComponentsMap().contains(compId));
    }

    TEST(SimDriverTest, AddNandGateSchedulesAndRunLoopSetsInitialOutputHigh) {
        DigitalSimDriver driver;
        const Bess::UUID compId(0xBADD);

        auto component = std::make_shared<DigSimComp>();
        std::atomic<bool> simulatedByRunLoop{false};

        auto def = std::make_shared<DigCompDef>();
        def->setName("NAND Gate");
        def->setSimFn([component, &simulatedByRunLoop](const std::shared_ptr<Drivers::SimFnDataBase> &dataBase) {
            const auto &data = std::dynamic_pointer_cast<DigCompSimData>(dataBase);

            const bool aHigh = data->inputStates.size() > 0 &&
                               data->inputStates[0].state == LogicState::high;
            const bool bHigh = data->inputStates.size() > 1 &&
                               data->inputStates[1].state == LogicState::high;
            const auto outState = (aHigh && bHigh) ? LogicState::low : LogicState::high;
            const SimTime slotTs =
                std::chrono::duration_cast<SimTime>(data->simTime);

            data->outputStates = std::vector<SlotState>{SlotState(outState, slotTs)};

            component->setOutputStates(data->outputStates);

            simulatedByRunLoop.store(true);

            return data;
        });

        component->setDefinition(def->clone());
        component->setUuid(compId);
        component->setName("NAND#1");
        component->setInputStates(std::vector<SlotState>{SlotState(LogicState::low, SimTime(0)),
                                                         SlotState(LogicState::low, SimTime(0))});
        component->setOutputStates(std::vector<SlotState>{SlotState(LogicState::low, SimTime(0))});

        driver.setState(Drivers::SimDriverState::running);
        std::thread runLoop([&driver]() {
            driver.run();
        });

        driver.addComponent(component);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (!simulatedByRunLoop.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        driver.stop();
        runLoop.join();

        const auto stored = driver.getComponent<DigSimComp>(compId);
        ASSERT_NE(stored, nullptr);
        ASSERT_EQ(stored->getOutputStates().size(), 1u);
        EXPECT_TRUE(simulatedByRunLoop.load());
        EXPECT_EQ(stored->getOutputStates()[0].state, LogicState::high);
    }

    TEST(SimDriverTest, SimulateReturnsFalseForUnknownComponent) {
        DigitalSimDriver driver;
        const Drivers::SimEvt evt{Bess::UUID(1), Bess::UUID(99), Bess::UUID::null, Bess::TimeNs(10.0)};

        EXPECT_FALSE(driver.simulate(evt));
    }

    TEST(SimDriverTest, AddingCompViaSimEngine) {
        auto &engine = SimulationEngine::instance();
        auto def = std::make_shared<DigCompDef>();
        def->setName("NAND Gate");
        def->setInputSlotsInfo({SlotsGroupType::input, false, 2, {"A", "B"}, {}});
        def->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"Y"}, {}});
        def->setSimFn([](const std::shared_ptr<Drivers::Digital::DigCompSimData> &dataBase) {
            return dataBase;
        });

        auto id = engine.addComponent(def);
        EXPECT_NE(id, Bess::UUID::null);

        auto comp = engine.getComponent<DigSimComp>(id);
        EXPECT_NE(comp, nullptr);
        EXPECT_EQ(comp->getUuid(), id);
        EXPECT_EQ(comp->getDefinition()->getName(), "NAND Gate");
        EXPECT_EQ(comp->getOutputStates().size(), 1u);

        engine.clear();
    }
} // namespace
