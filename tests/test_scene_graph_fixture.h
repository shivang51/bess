#pragma once

#include "command_system.h"
#include "component_catalog.h"
#include "drivers/digital_sim_driver.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/scene_components/conn_joint_scene_component.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "plugin_manager.h"
#include "scene/scene.h"
#include "scene/scene_ser_reg.h"
#include "simulation_engine.h"
#include <chrono>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <string_view>

namespace Bess::Tests {
    using namespace Bess::Canvas;
    using namespace Bess::SimEngine;

    inline std::shared_ptr<Drivers::CompDef> findDefinitionByName(std::string_view name) {
        const auto &components = ComponentCatalog::instance().getComponents();
        const auto it = std::ranges::find_if(components, [name](const auto &definition) {
            return definition && definition->getName() == name;
        });

        return it == components.end() ? nullptr : *it;
    }

    inline void ensurePrimitiveGateDefinitions() {
        auto ensureGate = [](const std::string &name,
                             size_t inputCount,
                             const std::function<LogicState(const std::vector<SlotState> &)> &eval) {
            if (findDefinitionByName(name)) {
                return;
            }

            auto definition = std::make_shared<Drivers::Digital::DigCompDef>();
            definition->setName(name);
            definition->setGroupName("Logic");
            definition->setInputSlotsInfo({SlotsGroupType::input, false, inputCount, {}, {}});
            definition->setOutputSlotsInfo({SlotsGroupType::output, false, 1, {}, {}});
            definition->setSimFn([eval](const std::shared_ptr<Drivers::Digital::DigCompSimData> &rawData)
                                     -> std::shared_ptr<Drivers::Digital::DigCompSimData> {
                const auto simData = std::dynamic_pointer_cast<Drivers::Digital::DigCompSimData>(rawData);
                if (!simData) {
                    return rawData;
                }

                if (simData->outputStates.empty()) {
                    simData->outputStates.resize(1);
                }

                const auto next = eval(simData->inputStates);
                const auto prev = simData->prevState.outputStates.empty()
                                      ? LogicState::unknown
                                      : simData->prevState.outputStates[0].state;

                simData->outputStates[0].state = next;
                simData->outputStates[0].lastChangeTime =
                    std::chrono::duration_cast<SimTime>(simData->simTime);
                simData->simDependants = prev != next;
                return simData;
            });
            definition->setPropDelay(Bess::TimeNs(1));
            ComponentCatalog::instance().registerComponent(definition);
        };

        ensureGate("NOT Gate", 1, [](const std::vector<SlotState> &inputs) {
            const auto inState = inputs.empty() ? LogicState::low : inputs[0].state;
            return inState == LogicState::high ? LogicState::low : LogicState::high;
        });
        ensureGate("AND Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a = inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b = inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a && b)
                       ? LogicState::high
                       : LogicState::low;
        });
        ensureGate("OR Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a = inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b = inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a || b)
                       ? LogicState::high
                       : LogicState::low;
        });
        ensureGate("XOR Gate", 2, [](const std::vector<SlotState> &inputs) {
            const bool a = inputs.size() > 0 && inputs[0].state == LogicState::high;
            const bool b = inputs.size() > 1 && inputs[1].state == LogicState::high;
            return (a != b) ? LogicState::high : LogicState::low;
        });
    }

    struct SimCompFixture {
        std::shared_ptr<SimulationSceneComponent> comp;
        std::vector<std::shared_ptr<SlotSceneComponent>> inputs;
        std::vector<std::shared_ptr<SlotSceneComponent>> outputs;

        std::shared_ptr<SlotSceneComponent> firstInput() const {
            const auto it = std::ranges::find_if(inputs, [](const auto &slot) {
                return slot && !slot->isResizeSlot();
            });
            return it == inputs.end() ? nullptr : *it;
        }

        std::shared_ptr<SlotSceneComponent> firstOutput() const {
            const auto it = std::ranges::find_if(outputs, [](const auto &slot) {
                return slot && !slot->isResizeSlot();
            });
            return it == outputs.end() ? nullptr : *it;
        }
    };

    class SceneGraphTestBase : public testing::Test {
      protected:
        static void SetUpTestSuite() {
            Bess::Plugins::PluginManager::getInstance().loadPluginsFromDirectory("plugins");
            Canvas::NonSimSceneComponent::registerComponent<Canvas::TextComponent>("Text Component");

            Canvas::SceneSerReg::registerComponent(Canvas::ConnJointSceneComp::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::ConnJointSceneComp::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::ConnectionSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::ConnectionSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::GroupSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::GroupSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::InputSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::InputSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::NonSimSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::NonSimSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::SimulationSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::SimulationSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::SlotSceneComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::SlotSceneComponent::fromJson(j);
                                                   });
            Canvas::SceneSerReg::registerComponent(Canvas::TextComponent::getStaticTypeName(),
                                                   [](const Json::Value &j) {
                                                       return Canvas::TextComponent::fromJson(j);
                                                   });
        }

        void SetUp() override {
            auto &simEngine = SimulationEngine::instance();
            simEngine.clear();

            ensurePrimitiveGateDefinitions();

            service = &Bess::Svc::SvcConnection::instance();
            service->destroy();
            service->init();

            copyPaste = &Bess::Svc::CopyPaste::Context::instance();
            copyPaste->destroy();
            copyPaste->init();

            inputDef = findDefinitionByName("Input");
            outputDef = findDefinitionByName("Output");
            notDef = findDefinitionByName("NOT Gate");
            andDef = findDefinitionByName("AND Gate");
            orDef = findDefinitionByName("OR Gate");

            ASSERT_NE(inputDef, nullptr);
            ASSERT_NE(outputDef, nullptr);
            ASSERT_NE(notDef, nullptr);
            ASSERT_NE(andDef, nullptr);
            ASSERT_NE(orDef, nullptr);

            scene = std::make_shared<Scene>();

            cmdSystem.init();
            cmdSystem.setScene(scene.get());
            cmdSystem.setSimEngine(&SimulationEngine::instance());
        }

        void TearDown() override {
            copyPaste->destroy();
            service->destroy();
            if (scene) {
                scene->clear();
                scene.reset();
            }
            SimulationEngine::instance().clear();
        }

        SimCompFixture addSimComponentDirect(const std::shared_ptr<Scene> &targetScene,
                                             const std::shared_ptr<Drivers::CompDef> &definition) {
            auto created = SimulationSceneComponent::createNew(definition);
            SimCompFixture fixture;
            fixture.comp = std::dynamic_pointer_cast<SimulationSceneComponent>(created.front());
            auto &state = targetScene->getState();
            state.addComponent(fixture.comp);

            for (size_t i = 1; i < created.size(); ++i) {
                auto slot = std::dynamic_pointer_cast<SlotSceneComponent>(created[i]);
                if (!slot) {
                    continue;
                }
                state.addComponent(slot);
                state.attachChild(fixture.comp->getUuid(), slot->getUuid(), false);
                if (slot->isInputSlot()) {
                    fixture.inputs.push_back(slot);
                } else {
                    fixture.outputs.push_back(slot);
                }
            }

            return fixture;
        }

        SimCompFixture executeAddSimComponent(const std::shared_ptr<Drivers::CompDef> &definition) {
            auto created = SimulationSceneComponent::createNew(definition);
            SimCompFixture fixture;
            fixture.comp = std::dynamic_pointer_cast<SimulationSceneComponent>(created.front());

            std::vector<std::shared_ptr<SceneComponent>> children;
            for (size_t i = 1; i < created.size(); ++i) {
                auto slot = std::dynamic_pointer_cast<SlotSceneComponent>(created[i]);
                if (!slot) {
                    continue;
                }
                children.push_back(slot);
                if (slot->isInputSlot()) {
                    fixture.inputs.push_back(slot);
                } else {
                    fixture.outputs.push_back(slot);
                }
            }

            cmdSystem.execute(std::make_unique<Bess::Cmd::AddCompCmd<SimulationSceneComponent>>(fixture.comp, children));
            return fixture;
        }

        std::shared_ptr<ConnectionSceneComponent> connectSlots(const std::shared_ptr<SlotSceneComponent> &src,
                                                               const std::shared_ptr<SlotSceneComponent> &dst) {
            auto connection = service->createConnection(src->getUuid(), dst->getUuid(), scene.get());
            EXPECT_NE(connection, nullptr);
            return connection;
        }

        std::shared_ptr<TextComponent> addTextComponentDirect(const std::shared_ptr<Scene> &targetScene,
                                                              const std::string &text,
                                                              const glm::vec3 &position = {}) {
            auto component = std::make_shared<TextComponent>();
            component->setData(text);
            component->setName(text);
            component->setPosition(position);
            targetScene->getState().addComponent(component);
            return component;
        }

        Bess::Svc::SvcConnection *service = nullptr;
        Bess::Svc::CopyPaste::Context *copyPaste = nullptr;
        std::shared_ptr<Drivers::CompDef> inputDef;
        std::shared_ptr<Drivers::CompDef> outputDef;
        std::shared_ptr<Drivers::CompDef> notDef;
        std::shared_ptr<Drivers::CompDef> andDef;
        std::shared_ptr<Drivers::CompDef> orDef;
        std::shared_ptr<Scene> scene;
        Bess::Cmd::CommandSystem cmdSystem;
    };
} // namespace Bess::Tests
