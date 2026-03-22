#include "module_scene_component.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "icons/FontAwesomeIcons.h"
#include "module_def.h"
#include "pages/main_page/cmds/module_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/scene_driver.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "scene/scene_state/scene_state.h"
#include "simulation_engine.h"
#include "types.h"
#include <memory>
#include <unordered_map>

namespace Bess::Canvas {
    ModuleSceneComponent::ModuleSceneComponent() {
        m_icon = UI::Icons::FontAwesomeIcons::FA_CUBES;
    };

    std::vector<std::shared_ptr<SceneComponent>> ModuleSceneComponent::clone(
        const SceneState &sceneState) const {
        auto moduleClone = std::make_shared<ModuleSceneComponent>(*this);
        auto clonedComps = cloneSimulationComponent(sceneState, moduleClone);

        const auto &clonedModDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(
            moduleClone->getCompDef());

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneDriver = mainPageState.getSceneDriver();

        auto newScene = sceneDriver.createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);

        moduleClone->setSceneId(newSceneState.getSceneId());
        newSceneState.setModuleId(moduleClone->getUuid());

        std::unordered_map<UUID, UUID> ogToCloneId = {};

        // Copying comps from old scene to new
        {
            auto ogScene = sceneDriver.getSceneWithId(m_sceneId);
            ogScene->selectAllEntities();
            Svc::CopyPaste::Context cpCtx;
            cpCtx.init();
            cpCtx.copy(ogScene);
            ogToCloneId = cpCtx.paste(newScene, false);
            cpCtx.destroy();
            ogScene->getState().clearSelectedComponents();
        }

        BESS_ASSERT(ogToCloneId.contains(m_associatedInp),
                    "[CloneModule] Associated input cloned mapping not found");

        BESS_ASSERT(ogToCloneId.contains(m_associatedOut),
                    "[CloneModule] Associated output cloned mapping not found");

        const auto &clonedInpId = ogToCloneId.at(m_associatedInp);
        const auto &clonedOutId = ogToCloneId.at(m_associatedOut);

        auto &simEngine = SimEngine::SimulationEngine::instance();

        // becuase onAttach simulation scene component creates its own dig comp in simulation engine
        auto clonedInp = newSceneState.getComponentByUuid<SimulationSceneComponent>(clonedInpId);
        simEngine.deleteComponent(clonedModDef->getInputId());
        clonedModDef->setInputId(clonedInp->getSimEngineId());
        auto clonedOut = newSceneState.getComponentByUuid<SimulationSceneComponent>(clonedOutId);
        simEngine.deleteComponent(clonedModDef->getOutputId());
        clonedModDef->setOutputId(clonedOut->getSimEngineId());

        return clonedComps;
    }

    void ModuleSceneComponent::setCallbacks(const SceneState &state) {
        const auto &simEngine = SimEngine::SimulationEngine::instance();
        auto moduleDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(m_compDef);
        BESS_ASSERT(moduleDef, "[ModuleSceneComponent] Module definition not found while setting callbacks");

        auto outputDigitalComp = simEngine.getDigitalComponent(moduleDef->getOutputId());
        auto inputDigitalComp = simEngine.getDigitalComponent(moduleDef->getInputId());
        auto moduleDigComp = simEngine.getDigitalComponent(m_simEngineId);

        if (!outputDigitalComp || !inputDigitalComp || !moduleDigComp) {
            BESS_ERROR("[ModuleSceneComponent] Failed to bind callbacks for module {}. Missing sim components. module={}, input={}, output={}",
                       m_name,
                       (uint64_t)m_simEngineId,
                       (uint64_t)moduleDef->getInputId(),
                       (uint64_t)moduleDef->getOutputId());
            return;
        }

        outputDigitalComp->clearCallbacks();
        outputDigitalComp->addOnStateChangeCB([this](const SimEngine::ComponentState &oldState,
                                                     const SimEngine::ComponentState &newState) {
            auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }
            int i = 0;
            for (auto state : newState.inputStates) {
                simEngine.setOutputSlotState(this->m_simEngineId, i++, state.state);
            }
        });

        outputDigitalComp->addOnInputSlotCountChangeCB([this](size_t newCount) {
            const auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }
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

        inputDigitalComp->clearCallbacks();
        inputDigitalComp->addOnOutputSlotCountChangeCB([this](size_t newCount) {
            const auto &simEngine = SimEngine::SimulationEngine::instance();
            auto moduleDigComp = simEngine.getDigitalComponent(this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }
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

    void ModuleSceneComponent::onAttach(SceneState &state) {
        SimulationSceneComponent::onAttach(state);
        setCallbacks(state);
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state, UUID caller) {
        return {};
    }

    std::vector<std::shared_ptr<SceneComponent>> ModuleSceneComponent::createNew(UUID &moduleInpId,
                                                                                 UUID &moduleOutId) {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto &sceneDriver = mainPageState.getSceneDriver();

        auto newScene = sceneDriver.createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);

        auto moduleDef = SimEngine::ModuleDefinition::createNew();
        auto comps = SimulationSceneComponent::createNew<ModuleSceneComponent>(moduleDef);

        auto moduleComp = std::dynamic_pointer_cast<ModuleSceneComponent>(comps.front());
        moduleComp->setSceneId(newSceneState.getSceneId());
        moduleComp->m_transform.position.z = sceneDriver->getNextZCoord();
        newSceneState.setModuleId(moduleComp->getUuid());

        const auto &simEngine = SimEngine::SimulationEngine::instance();

        // adding io to new module scene

        const auto inpDef = simEngine.getComponentDefinition(moduleDef->getInputId());
        auto inpComps = SimulationSceneComponent::createNew<SimulationSceneComponent>(inpDef);
        const auto inpSceneComp = std::dynamic_pointer_cast<SimulationSceneComponent>(inpComps.front());
        inpSceneComp->setName("Module Input");
        inpSceneComp->getTransform().position.z = newScene->getNextZCoord();
        inpSceneComp->getTransform().position.x = -200.f;
        inpSceneComp->setSimEngineId(moduleDef->getInputId());
        moduleInpId = inpSceneComp->getUuid();
        moduleComp->m_associatedInp = inpSceneComp->getUuid();
        inpComps.erase(inpComps.begin());

        // comps.insert(comps.end(), inpComps.begin(), inpComps.end());

        newSceneState.addComponent(inpSceneComp, true, false);

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
        outSceneComp->setSimEngineId(moduleDef->getOutputId());
        moduleOutId = outSceneComp->getUuid();
        moduleComp->m_associatedOut = outSceneComp->getUuid();
        outComps.erase(outComps.begin());
        // comps.insert(comps.end(), outComps.begin(), outComps.end());

        newSceneState.addComponent(outSceneComp, true, false);
        for (const auto &outComp : outComps) {
            newSceneState.addComponent(outComp, true, false);
            newSceneState.attachChild(outSceneComp->getUuid(), outComp->getUuid(), false);
        }

        return comps;
    }

    std::shared_ptr<ModuleSceneComponent> ModuleSceneComponent::fromNet(const UUID &netId,
                                                                        const std::string &name) {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto command = std::make_unique<Cmd::CreateModuleCmd>(mainPageState.getSceneDriver().getActiveScene(),
                                                              netId,
                                                              name);
        auto *commandPtr = command.get();
        mainPageState.getCommandSystem().execute(std::move(command));
        return commandPtr->getModuleComponent();
    }

    void ModuleSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button == Canvas::Events::MouseButton::left &&
            e.action == Canvas::Events::MouseClickAction::doubleClick) {
            auto &driver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            driver.setActiveScene(m_sceneId);
        }
    }
} // namespace Bess::Canvas
