#include "drivers/digital_sim_driver.h"
#include "drivers/sim_driver.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <thread>

namespace {
	using namespace Bess::SimEngine;
	using namespace Bess::SimEngine::Digital;

	TEST(SimDriverTest, CreateBaseDefinitionStoresTypeName) {
		ComponentDef definition;
		definition.setTypeName("base.definition");

		const auto json = definition.toJson();

		EXPECT_EQ(definition.getTypeName(), "base.definition");
		ASSERT_TRUE(json.isMember("typeName"));
		EXPECT_EQ(json["typeName"].asString(), "base.definition");
	}

	TEST(SimDriverTest, CreateAndGateDefinitionHasExpectedShape) {
		ComponentDefinition andGate;
		andGate.setName("AND Gate");
		andGate.setGroupName("Logic");
		andGate.setInputSlotsInfo({SlotsGroupType::input, false, 2, {"A", "B"}, {}});
		andGate.setOutputSlotsInfo({SlotsGroupType::output, false, 1, {"Y"}, {}});
		andGate.setOpInfo({'*', false});
		andGate.setShouldAutoReschedule(false);
		andGate.setSimDelay(SimDelayNanoSeconds(1));
		andGate.setSimulationFunction([](const std::vector<SlotState> &inputs,
										 SimTime ts,
										 const ComponentState &prevState) {
			auto next = prevState;
			next.inputStates = inputs;
			next.outputStates.resize(1);
			const bool out = inputs.size() >= 2 &&
							 inputs[0].state == LogicState::high &&
							 inputs[1].state == LogicState::high;
			next.outputStates[0] = SlotState(out ? LogicState::high : LogicState::low, ts);
			next.isChanged = true;
			return next;
		});

		EXPECT_EQ(andGate.getName(), "AND Gate");
		EXPECT_EQ(andGate.getGroupName(), "Logic");
		EXPECT_EQ(andGate.getInputSlotsInfo().count, 2u);
		EXPECT_EQ(andGate.getOutputSlotsInfo().count, 1u);
		EXPECT_EQ(andGate.getOpInfo().op, '*');
		EXPECT_FALSE(andGate.getShouldAutoReschedule());
		EXPECT_EQ(andGate.getSimDelay(), SimDelayNanoSeconds(1));
		EXPECT_TRUE(static_cast<bool>(andGate.getSimFunctionCopy()));

		const auto simFn = andGate.getSimFunctionCopy();
		ComponentState prev;
		prev.outputStates.resize(1);
		const auto next = simFn({SlotState(true), SlotState(true)}, SimTime(10), prev);
		ASSERT_EQ(next.outputStates.size(), 1u);
		EXPECT_EQ(next.outputStates[0].state, LogicState::high);
	}

	TEST(SimDriverTest, AddComponentRegistersItInDigitalDriverMap) {
		DigitalSimDriver driver;
		const Bess::UUID compId(0xAA11);

		auto component = std::make_shared<DigitalSimComponent>();
		component->setUuid(compId);
		component->setName("AND#1");

		EXPECT_EQ(driver.getComponent<DigitalSimComponent>(compId), nullptr);

		driver.addComponent(component);

		const auto stored = driver.getComponent<DigitalSimComponent>(compId);
		ASSERT_NE(stored, nullptr);
		EXPECT_EQ(stored->getUuid(), compId);
		EXPECT_EQ(driver.getComponentsMap().size(), 1u);
		EXPECT_TRUE(driver.getComponentsMap().contains(compId));
	}

	TEST(SimDriverTest, SimulateDelegatesToRegisteredComponent) {
		DigitalSimDriver driver;
		const Bess::UUID compId(0xCAFE);

		auto component = std::make_shared<DigitalSimComponent>();
		component->setUuid(compId);
		component->setInputStates(std::vector<SlotState>{SlotState(LogicState::high, SimTime(7))});
		component->setOutputStates(std::vector<SlotState>{SlotState(LogicState::low, SimTime(3))});

		bool called = false;
		component->setOnSimulate([&called](const DigCompSimData &data) {
			called = true;
			EXPECT_EQ(data.prevState.inputStates.size(), 1u);
			EXPECT_EQ(data.prevState.outputStates.size(), 1u);
			return true;
		});

		driver.addComponent(component);

		const SimEvt evt{Bess::UUID(1), compId, Bess::UUID::null, Bess::TimeNs(10.0)};
		EXPECT_TRUE(driver.simulate(evt));
		EXPECT_TRUE(called);
	}

	TEST(SimDriverTest, AddNandGateSchedulesAndRunLoopSetsInitialOutputHigh) {
		DigitalSimDriver driver;
		const Bess::UUID compId(0xBADD);

		auto component = std::make_shared<DigitalSimComponent>();
		component->setUuid(compId);
		component->setName("NAND#1");
		component->setInputStates(std::vector<SlotState>{SlotState(LogicState::low, SimTime(0)),
													 SlotState(LogicState::low, SimTime(0))});
		component->setOutputStates(std::vector<SlotState>{SlotState(LogicState::low, SimTime(0))});

		std::atomic<bool> simulatedByRunLoop{false};
		component->setOnSimulate([component, &simulatedByRunLoop](const DigCompSimData &data) {
			simulatedByRunLoop.store(true);

			const bool aHigh = data.inputStates.size() > 0 &&
							  data.inputStates[0].state == LogicState::high;
			const bool bHigh = data.inputStates.size() > 1 &&
							  data.inputStates[1].state == LogicState::high;
			const auto outState = (aHigh && bHigh) ? LogicState::low : LogicState::high;
			const SimTime slotTs =
				std::chrono::duration_cast<SimTime>(data.simTime);

			component->setOutputStates(
				std::vector<SlotState>{SlotState(outState, slotTs)});
			return true;
		});

		driver.setState(SimDriverState::running);
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

		const auto stored = driver.getComponent<DigitalSimComponent>(compId);
		ASSERT_NE(stored, nullptr);
		ASSERT_EQ(stored->getOutputStates().size(), 1u);
		EXPECT_TRUE(simulatedByRunLoop.load());
		EXPECT_EQ(stored->getOutputStates()[0].state, LogicState::high);
	}

	TEST(SimDriverTest, SimulateReturnsFalseForUnknownComponent) {
		DigitalSimDriver driver;
		const SimEvt evt{Bess::UUID(1), Bess::UUID(99), Bess::UUID::null, Bess::TimeNs(10.0)};

		EXPECT_FALSE(driver.simulate(evt));
	}
} // namespace
