#pragma once

#include "command_system.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "plugin_manager.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include <gtest/gtest.h>
#include <memory>
#include <ranges>
#include <string_view>

namespace Bess::Tests {
    using namespace Bess::Canvas;
    using namespace Bess::SimEngine;

    inline std::shared_ptr<ComponentDefinition> findDefinitionByName(std::string_view name) {
        const auto &components = ComponentCatalog::instance().getComponents();
        const auto it = std::ranges::find_if(components, [name](const auto &definition) {
            return definition && definition->getName() == name;
        });

        return it == components.end() ? nullptr : *it;
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
        }

        void SetUp() override {
            auto &simEngine = SimulationEngine::instance();
            simEngine.clear();

            service = &Bess::Svc::SvcConnection::instance();
            service->destroy();
            service->init();

            copyPaste = &Bess::Svc::CopyPaste::Context::instance();
            copyPaste->destroy();
            copyPaste->init();

            inputDef = findDefinitionByName("Input");
            outputDef = findDefinitionByName("Output");

            ASSERT_NE(inputDef, nullptr);
            ASSERT_NE(outputDef, nullptr);

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
            SimulationEngine::instance().destroy();
        }

        SimCompFixture addSimComponentDirect(const std::shared_ptr<Scene> &targetScene,
                                             const std::shared_ptr<ComponentDefinition> &definition) {
            auto created = SimulationSceneComponent::createNewAndRegister(definition);
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

        SimCompFixture executeAddSimComponent(const std::shared_ptr<ComponentDefinition> &definition) {
            auto created = SimulationSceneComponent::createNewAndRegister(definition);
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

        Bess::Svc::SvcConnection *service = nullptr;
        Bess::Svc::CopyPaste::Context *copyPaste = nullptr;
        std::shared_ptr<ComponentDefinition> inputDef;
        std::shared_ptr<ComponentDefinition> outputDef;
        std::shared_ptr<Scene> scene;
        Bess::Cmd::CommandSystem cmdSystem;
    };
} // namespace Bess::Tests
