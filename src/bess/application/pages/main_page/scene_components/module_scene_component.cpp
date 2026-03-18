#include "module_scene_component.h"
#include "common/bess_uuid.h"
#include "module_def.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/scene_driver.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"
#include <memory>

namespace Bess::Canvas {

    void ModuleSceneComponent::onAttach(SceneState &state) {
        SimulationSceneComponent::onAttach(state);

        const auto &simEngine = SimEngine::SimulationEngine::instance();
        auto moduleDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(m_compDef);
        auto outputDigitalComp = simEngine.getDigitalComponent(moduleDef->getOutputId());
        auto inputDigitalComp = simEngine.getDigitalComponent(moduleDef->getInputId());

        outputDigitalComp->addOnStateChangeCB([this](const SimEngine::ComponentState &oldState,
                                                     const SimEngine::ComponentState &newState) {
            auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            int i = 0;
            for (auto state : newState.inputStates) {
                simEngine.setOutputSlotState(this->m_simEngineId, i++, state.state);
            }
        });

        outputDigitalComp->addOnInputSlotCountChangeCB([this](size_t newCount) {
            BESS_TRACE("Resizing Slots to {}", newCount);
            const auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            const auto currCount = moduleDigComp->definition->getOutputSlotsInfo().count;

            const auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            auto rootScene = sceneDriver.getSceneWithId(sceneDriver.getRootSceneId());
            auto &rootSceneState = rootScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    moduleDigComp->incrementOutputCount(true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setSlotType(SlotType::digitalOutput);
                    m_outputSlots.push_back(slot->getUuid());
                    rootSceneState.addComponent(slot, false, false);
                    rootSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    moduleDigComp->decrementOutputCount(true);
                    rootSceneState.removeComponent(m_outputSlots.back(), m_uuid);
                    removeChildComponent(m_outputSlots.back());
                    m_outputSlots.pop_back();
                }
            }

            m_isScaleDirty = true;

            const auto modOutCount = moduleDigComp->definition->getOutputSlotsInfo().count;
            BESS_ASSERT(modOutCount == newCount, "Failed to sync module inputs");
        });

        inputDigitalComp->addOnOutputSlotCountChangeCB([this](size_t newCount) {
            BESS_TRACE("Resizing Inp Slots to {}", newCount);
            const auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            const auto currCount = moduleDigComp->definition->getInputSlotsInfo().count;

            const auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            auto rootScene = sceneDriver.getSceneWithId(sceneDriver.getRootSceneId());
            auto &rootSceneState = rootScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    moduleDigComp->incrementInputCount(true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setSlotType(SlotType::digitalInput);
                    m_inputSlots.push_back(slot->getUuid());
                    rootSceneState.addComponent(slot, false, false);
                    rootSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    moduleDigComp->decrementInputCount(true);
                    rootSceneState.removeComponent(m_inputSlots.back(), m_uuid);
                    removeChildComponent(m_inputSlots.back());
                    m_inputSlots.pop_back();
                }
            }

            const auto modInpCount = moduleDigComp->definition->getInputSlotsInfo().count;
            BESS_ASSERT(modInpCount == newCount, "Failed to sync module inputs");
        });
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state, UUID caller) {
        return {};
    }

    void ModuleSceneComponent::onSelect() {
    }

    std::shared_ptr<ModuleSceneComponent> ModuleSceneComponent::fromNet(const UUID &netId,
                                                                        const std::string &name) {

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneDriver = mainPageState.getSceneDriver();
        auto &sceneState = sceneDriver->getState();
        auto &netCompMap = mainPageState.getNetIdToCompMap(sceneDriver->getState().getSceneId());

        if (!netCompMap.contains(netId) || netCompMap[netId].empty()) {
            BESS_WARN("[ModuleSceneComponent] No components found for netId {}, cannot create module component.",
                      (uint64_t)netId);
            return nullptr;
        }

        const auto &compIds = netCompMap.at(netId);

        auto newScene = sceneDriver.createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);

        auto moduleDef = SimEngine::ModuleDefinition::createNew();
        auto comps = SimulationSceneComponent::createNew<ModuleSceneComponent>(moduleDef);
        BESS_TRACE("Created module component with {} subcomponents", comps.size());

        auto moduleComp = std::dynamic_pointer_cast<ModuleSceneComponent>(comps.front());
        comps.erase(comps.begin());
        sceneState.addComponent(moduleComp);
        for (const auto &comp : comps) {
            sceneState.addComponent(comp);
            sceneState.attachChild(moduleComp->getUuid(), comp->getUuid());
        }

        moduleComp->setSceneId(newSceneState.getSceneId());
        newSceneState.setModuleId(moduleComp->getUuid());

        auto &style = moduleComp->getStyle();
        style.color = ViewportTheme::colors.componentBG;
        style.headerColor = ViewportTheme::colors.componentBG;
        style.borderRadius = glm::vec4(6.f);
        style.borderColor = ViewportTheme::colors.componentBorder;
        style.borderSize = glm::vec4(1.f);
        style.color = ViewportTheme::colors.componentBG;

        std::vector<std::shared_ptr<SceneComponent>> compsToMove;

        // move components and their dependants to new scene
        {
            std::unordered_set<UUID> visited;
            std::function<void(const UUID &)> collect = [&](const UUID &uuid) {
                if (visited.contains(uuid))
                    return;
                auto comp = sceneState.getComponentByUuid(uuid);
                if (!comp)
                    return;

                visited.insert(uuid);
                for (const auto &depUuid : comp->getDependants(sceneState)) {
                    collect(depUuid);
                }
                compsToMove.push_back(comp);
            };

            for (const auto &startUuid : compIds) {
                collect(startUuid);
            }
        }

        // moving i.e. removing from the active scene to the new scene
        for (const auto &comp : compsToMove) {
            sceneState.removeFromMap(comp->getUuid());
            newSceneState.addComponent(comp, false, false);
        }

        const auto &simEngine = SimEngine::SimulationEngine::instance();

        // adding io to new module scene
        const auto inpDef = simEngine.getComponentDefinition(moduleDef->getInputId());
        auto inpComps = SimulationSceneComponent::createNew<SimulationSceneComponent>(inpDef);
        const auto inpSceneComp = std::dynamic_pointer_cast<SimulationSceneComponent>(inpComps.front());
        inpSceneComp->setName("Module Input");
        inpSceneComp->getTransform().position.z = newScene->getNextZCoord();
        inpSceneComp->getTransform().position.x = -200.f;
        inpComps.erase(inpComps.begin());
        newSceneState.addComponent(inpSceneComp, true, false);
        inpSceneComp->setSimEngineId(moduleDef->getInputId()); // Fixme: a temp fix as onAttach sets its own id
        for (const auto &inpComp : inpComps) {
            newSceneState.addComponent(inpComp, true, false);
            newSceneState.attachChild(inpSceneComp->getUuid(), inpComp->getUuid(), false);
        }

        auto outDef = simEngine.getComponentDefinition(moduleDef->getOutputId());
        auto outComps = SimulationSceneComponent::createNewAndRegister(outDef);
        auto outSceneComp = std::dynamic_pointer_cast<SimulationSceneComponent>(outComps.front());
        outSceneComp->setName("Module Output");
        outSceneComp->getTransform().position.z = newScene->getNextZCoord();
        outSceneComp->getTransform().position.x = 200.f;
        outComps.erase(outComps.begin());
        newSceneState.addComponent(outSceneComp, true, false);
        outSceneComp->setSimEngineId(moduleDef->getOutputId()); // Fixme: a temp fix as onAttach sets its own id
        for (const auto &outComp : outComps) {
            newSceneState.addComponent(outComp, true, false);
            newSceneState.attachChild(outSceneComp->getUuid(), outComp->getUuid(), false);
        }

        return moduleComp->cast<ModuleSceneComponent>();
    }

} // namespace Bess::Canvas
