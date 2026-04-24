#include "application/pages/main_page/services/hierarchical_scene_layout.h"
#include "drivers/digital_sim_driver.h"
#include "event_dispatcher.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "gtest/gtest.h"
#include <chrono>
#include <memory>

namespace {
    using namespace Bess::Canvas;
    using namespace Bess::SimEngine;

    std::shared_ptr<Drivers::ComponentDef> makeDefinition(const std::string &name,
                                                          ComponentBehaviorType behaviorType,
                                                          size_t inputCount,
                                                          size_t outputCount) {
        auto definition = std::make_shared<Drivers::Digital::DigCompDef>();
        definition->setName(name);
        definition->setBehaviorType(behaviorType);
        definition->setGroupName("Test");

        SlotsGroupInfo inputs;
        inputs.type = SlotsGroupType::input;
        inputs.count = inputCount;
        definition->setInputSlotsInfo(inputs);

        SlotsGroupInfo outputs;
        outputs.type = SlotsGroupType::output;
        outputs.count = outputCount;
        definition->setOutputSlotsInfo(outputs);

        definition->setSimFn(
            [outputCount](const std::shared_ptr<Drivers::SimFnDataBase> &rawData)
                -> std::shared_ptr<Drivers::SimFnDataBase> {
                const auto simData = std::dynamic_pointer_cast<Drivers::Digital::DigCompSimData>(rawData);
                if (!simData) {
                    return rawData;
                }

                simData->outputStates.resize(outputCount);

                for (size_t i = 0; i < outputCount; ++i) {
                    if (i < simData->inputStates.size()) {
                        simData->outputStates[i] = simData->inputStates[i];
                    } else if (!simData->inputStates.empty()) {
                        simData->outputStates[i] = simData->inputStates.front();
                    }
                }

                simData->simDependants = true;
                return simData;
            });

        return definition;
    }

    struct SimFixture {
        std::shared_ptr<SimulationSceneComponent> component;
    };

    SimFixture addSimulationComponent(Scene &scene,
                                      const std::shared_ptr<Drivers::ComponentDef> &definition) {
        auto created = SimulationSceneComponent::createNew(definition);
        auto component =
            std::dynamic_pointer_cast<SimulationSceneComponent>(created.front());

        auto &sceneState = scene.getState();
        sceneState.addComponent(component);

        for (size_t i = 1; i < created.size(); ++i) {
            sceneState.addComponent(created[i]);
            sceneState.attachChild(component->getUuid(), created[i]->getUuid(), false);
        }

        return {.component = component};
    }

    class HierarchicalLayoutTest : public testing::Test {
      protected:
        void SetUp() override {
            engine = &SimulationEngine::instance();
            engine->clear();
        }

        void TearDown() override {
            if (engine) {
                engine->clear();
                engine = nullptr;
            }
        }

        SimulationEngine *engine = nullptr;
    };
} // namespace

TEST_F(HierarchicalLayoutTest, PlacesConnectedComponentsInLeftToRightSignalOrder) {
    Scene scene;

    const auto inputDef =
        makeDefinition("Input", ComponentBehaviorType::input, 0, 1);
    const auto outputDef =
        makeDefinition("Output", ComponentBehaviorType::output, 1, 0);
    const auto inverterDef =
        makeDefinition("Inverter", ComponentBehaviorType::none, 1, 1);
    const auto andDef =
        makeDefinition("AndGate", ComponentBehaviorType::none, 2, 1);

    const auto inputA = addSimulationComponent(scene, inputDef);
    const auto inputB = addSimulationComponent(scene, inputDef);
    const auto inverter = addSimulationComponent(scene, inverterDef);
    const auto andGate = addSimulationComponent(scene, andDef);
    const auto output = addSimulationComponent(scene, outputDef);

    ASSERT_TRUE((engine->connectComponent(inputA.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalOutput,
                                          andGate.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalInput)));
    ASSERT_TRUE((engine->connectComponent(inputB.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalOutput,
                                          inverter.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalInput)));
    ASSERT_TRUE((engine->connectComponent(inverter.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalOutput,
                                          andGate.component->getSimEngineId(),
                                          1,
                                          Bess::SimEngine::SlotType::digitalInput)));
    ASSERT_TRUE((engine->connectComponent(andGate.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalOutput,
                                          output.component->getSimEngineId(),
                                          0,
                                          Bess::SimEngine::SlotType::digitalInput)));

    const auto result =
        Bess::Pages::applyHierarchicalSceneLayout(scene, *engine);

    ASSERT_TRUE(result.applied);
    EXPECT_EQ(result.laidOutNodes, 5u);
    EXPECT_GE(result.uniqueEdges, 4u);

    const auto inputAX = inputA.component->getTransform().position.x;
    const auto inputBX = inputB.component->getTransform().position.x;
    const auto inverterX = inverter.component->getTransform().position.x;
    const auto andX = andGate.component->getTransform().position.x;
    const auto outputX = output.component->getTransform().position.x;

    EXPECT_NEAR(inputAX, inputBX, 0.01f);
    EXPECT_LT(inputAX, inverterX);
    EXPECT_LT(inputAX, andX);
    EXPECT_LT(inverterX, andX);
    EXPECT_LT(andX, outputX);
}

TEST_F(HierarchicalLayoutTest, IgnoresQueuedEventsFromDestroyedUntrackedScenes) {
    {
        Scene scene;
        const auto inputDef =
            makeDefinition("Input", ComponentBehaviorType::input, 0, 1);
        addSimulationComponent(scene, inputDef);
    }

    Bess::EventSystem::EventDispatcher::instance().dispatchAll();
    SUCCEED();
}
