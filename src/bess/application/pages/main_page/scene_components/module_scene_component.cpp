#include "module_scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene_driver.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "dig_module_def.h"
#include "dig_sim_driver.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/ui_main/ui_main.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Bess::Canvas {
    namespace {
        glm::vec2 calculateCopiedSceneCenter(const SceneState &sceneState) {
            glm::vec2 sum{0.f, 0.f};
            size_t count = 0;

            for (const auto &componentId : sceneState.getRootComponents()) {
                const auto component =
                    sceneState.getComponentByUuid(componentId);
                if (!component) {
                    continue;
                }

                const auto &position = component->getTransform().position;
                sum += glm::vec2{position.x, position.y};
                ++count;
            }

            return count == 0 ? glm::vec2{0.f, 0.f}
                              : sum / static_cast<float>(count);
        }
    } // namespace

    ModuleSceneComponent::ModuleSceneComponent() {
        m_icon = Bess::UI::Icons::FontAwesomeIcons::FA_CUBES;
    };

    std::vector<std::shared_ptr<SceneComponent>>
    ModuleSceneComponent::clone(const SceneState &sceneState) const {
        auto moduleClone = std::make_shared<ModuleSceneComponent>(*this);
        auto clonedComps = cloneSimulationComponent(sceneState, moduleClone);

        const auto &clonedModDef =
            std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(
                moduleClone->getCompDef());

        auto *sceneDriver = sceneState.runtime().scenes;
        auto *simEngine = sceneState.runtime().sim;
        BESS_ASSERT(sceneDriver && simEngine,
                    "[CloneModule] Scene runtime is unavailable");
        if (!sceneDriver || !simEngine) {
            return {};
        }

        auto newScene = sceneDriver->createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);
        newSceneState.setParentSceneId(sceneState.getSceneId());
        newSceneState.setModuleId(moduleClone->getUuid());

        moduleClone->setSceneId(newSceneState.getSceneId());

        std::unordered_map<UUID, UUID> ogToCloneId = {};

        // Copying comps from old scene to new
        {
            auto ogScene = sceneDriver->getSceneWithId(m_sceneId);
            BESS_ASSERT(ogScene, "[CloneModule] Source module scene not found");
            if (!ogScene) {
                BESS_ERROR("[CloneModule] Source module scene {} not found",
                           (uint64_t)m_sceneId);
                sceneDriver->removeScene(newSceneState.getSceneId());
                return {};
            }

            Svc::CopyPaste::Context cpCtx;
            cpCtx.copyScene(ogScene);
            ogToCloneId =
                cpCtx.paste(newScene,
                            calculateCopiedSceneCenter(ogScene->getState()),
                            false);
        }

        BESS_ASSERT(ogToCloneId.contains(m_associatedInp),
                    "[CloneModule] Associated input cloned mapping not found");

        BESS_ASSERT(ogToCloneId.contains(m_associatedOut),
                    "[CloneModule] Associated output cloned mapping not found");

        const auto &clonedInpId = ogToCloneId.at(m_associatedInp);
        const auto &clonedOutId = ogToCloneId.at(m_associatedOut);

        moduleClone->setAssociatedInp(clonedInpId);
        moduleClone->setAssociatedOut(clonedOutId);

        // becuase onAttach simulation scene component creates its own dig comp
        // in simulation engine
        auto clonedInp =
            newSceneState.getComponentByUuid<SimulationSceneComponent>(
                clonedInpId);
        BESS_ASSERT(clonedInp,
                    "[CloneModule] Cloned associated input "
                    "component not found in new scene");
        simEngine->deleteComponent(clonedModDef->getInputId());
        clonedModDef->setInputId(clonedInp->getSimEngineId());
        auto clonedOut =
            newSceneState.getComponentByUuid<SimulationSceneComponent>(
                clonedOutId);
        BESS_ASSERT(clonedOut,
                    "[CloneModule] Cloned associated output "
                    "component not found in new scene");
        simEngine->deleteComponent(clonedModDef->getOutputId());
        clonedModDef->setOutputId(clonedOut->getSimEngineId());

        return clonedComps;
    }

    void ModuleSceneComponent::setCallbacks(const SceneState &state) {
        auto *simEngine = state.runtime().sim;
        auto *sceneDriver = state.runtime().scenes;
        BESS_ASSERT(simEngine && sceneDriver,
                    "[ModuleSceneComponent] Scene runtime is unavailable");
        if (!simEngine || !sceneDriver) {
            return;
        }
        auto moduleDef =
            std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(m_compDef);
        BESS_ASSERT(moduleDef,
                    "[ModuleSceneComponent] Module definition not "
                    "found while setting callbacks");
        const auto ownerSceneId = state.getSceneId();

        const auto &outputDigitalComp =
            simEngine->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                moduleDef->getOutputId());
        const auto &inputDigitalComp =
            simEngine->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                moduleDef->getInputId());
        auto moduleDigComp =
            simEngine->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                m_simEngineId);

        BESS_ASSERT(outputDigitalComp,
                    "[ModuleSceneComponent] Missing output sim component {} "
                    "for module {}",
                    (uint64_t)moduleDef->getOutputId(),
                    (uint64_t)m_uuid);

        BESS_ASSERT(inputDigitalComp,
                    "[ModuleSceneComponent] Missing input sim component {} for "
                    "module {}",
                    (uint64_t)moduleDef->getInputId(),
                    (uint64_t)m_uuid);

        BESS_ASSERT(moduleDigComp,
                    "[ModuleSceneComponent] Missing module sim component {} "
                    "for module {}",
                    (uint64_t)m_simEngineId,
                    (uint64_t)m_uuid);

        if (!outputDigitalComp || !inputDigitalComp || !moduleDigComp) {
            BESS_ERROR(
                "[ModuleSceneComponent] Failed to bind callbacks for module "
                "{}. Missing sim components. module={}, input={}, output={}",
                m_name,
                (uint64_t)m_simEngineId,
                (uint64_t)moduleDef->getInputId(),
                (uint64_t)moduleDef->getOutputId());
            return;
        }

        outputDigitalComp->removeOnStateChangeCB(m_uuid);
        outputDigitalComp->addOnStateChangeCB(
            m_uuid,
            [this,
             simEngine](const std::vector<SimEngine::PortState> &inputStates,
                        const std::vector<SimEngine::PortState> &outputStates) {
                auto moduleDigComp =
                    simEngine
                        ->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                            this->m_simEngineId);
                if (!moduleDigComp) {
                    return;
                }

                const auto maxOutputs = moduleDigComp->getOutputStates().size();
                const auto copyCount = std::min(maxOutputs, inputStates.size());

                auto &outputs = moduleDigComp->getOutputStates();
                for (size_t i = 0; i < copyCount; ++i) {
                    outputs[i] = inputStates[i];
                }

                if (!outputs.empty()) { // to schedule sim event
                    simEngine->setOutputPortState(
                        this->m_simEngineId, 0, outputs[0].getLogicState());
                }
            });

        auto onOutputSlotChange = [this, ownerSceneId, simEngine, sceneDriver](
                                      const UUID &id,
                                      SimEngine::PortDirection direction,
                                      SimEngine::SignalKind signalKind,
                                      int newCount) {
            (void)id;
            if (direction != SimEngine::PortDirection::output ||
                signalKind != SimEngine::SignalKind::digital) {
                return;
            }

            auto moduleDigComp =
                simEngine
                    ->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                        this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }

            const auto moduleDef =
                moduleDigComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            const auto currCount = moduleDef->getOutputSlotsInfo().count;

            const auto ownerScene = sceneDriver->getSceneWithId(ownerSceneId);
            if (!ownerScene) {
                return;
            }
            auto &ownerSceneState = ownerScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    simEngine->addPort(
                        {.componentId = this->m_simEngineId,
                         .direction = SimEngine::PortDirection::output,
                         .signalKind = SimEngine::SignalKind::digital,
                         .index = (int)i},
                        true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setPortDirection(SimEngine::PortDirection::output);
                    slot->setSignalKind(SimEngine::SignalKind::digital);
                    slot->setResizeTrigger(false);
                    m_outputSlots.push_back(slot->getUuid());
                    ownerSceneState.addComponent(slot, false, false);
                    ownerSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    simEngine->removePort(
                        {.componentId = this->m_simEngineId,
                         .direction = SimEngine::PortDirection::output,
                         .signalKind = SimEngine::SignalKind::digital,
                         .index = (int)i},
                        true);
                    ownerSceneState.removeComponent(m_outputSlots.back(),
                                                    m_uuid);
                    removeChildComponent(m_outputSlots.back());
                    m_outputSlots.pop_back();
                }
            }

            setScaleDirty();
            setSchematicScaleDirty();

            const auto modOutCount = moduleDef->getOutputSlotsInfo().count;
            BESS_ASSERT(modOutCount == newCount,
                        "Failed to sync module inputs");
        };

        auto onInputSlotChange = [this, ownerSceneId, simEngine, sceneDriver](
                                     const UUID &id,
                                     SimEngine::PortDirection direction,
                                     SimEngine::SignalKind signalKind,
                                     int newCount) {
            (void)id;
            if (direction != SimEngine::PortDirection::input ||
                signalKind != SimEngine::SignalKind::digital) {
                return;
            }

            auto moduleDigComp =
                simEngine
                    ->getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                        this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }

            const auto moduleDef =
                moduleDigComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            const auto currCount = moduleDef->getInputSlotsInfo().count;

            const auto ownerScene = sceneDriver->getSceneWithId(ownerSceneId);
            if (!ownerScene) {
                return;
            }
            auto &ownerSceneState = ownerScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    simEngine->addPort(
                        {.componentId = this->m_simEngineId,
                         .direction = SimEngine::PortDirection::input,
                         .signalKind = SimEngine::SignalKind::digital,
                         .index = (int)i},
                        true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setPortDirection(SimEngine::PortDirection::input);
                    slot->setSignalKind(SimEngine::SignalKind::digital);
                    slot->setResizeTrigger(false);
                    m_inputSlots.push_back(slot->getUuid());
                    ownerSceneState.addComponent(slot, false, false);
                    ownerSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    simEngine->removePort(
                        {.componentId = this->m_simEngineId,
                         .direction = SimEngine::PortDirection::input,
                         .signalKind = SimEngine::SignalKind::digital,
                         .index = (int)i},
                        true);
                    ownerSceneState.removeComponent(m_inputSlots.back(),
                                                    m_uuid);
                    removeChildComponent(m_inputSlots.back());
                    m_inputSlots.pop_back();
                }
            }

            setScaleDirty();
            setSchematicScaleDirty();

            const auto modInpCount = moduleDef->getInputSlotsInfo().count;

            BESS_ASSERT(modInpCount == newCount,
                        "Failed to sync module inputs");
        };

        // slot count: to sync module io slots and associated inp and output
        // comp slots
        simEngine->removeOnPortCountChangeCB(m_simEngineId);
        simEngine->addOnPortCountChangeCB(
            m_simEngineId,
            [inputDigitalComp,
             outputDigitalComp,
             onInputSlotChange,
             onOutputSlotChange](const UUID &id,
                                 SimEngine::PortDirection direction,
                                 SimEngine::SignalKind signalKind,
                                 int newCount) {
                if (id == inputDigitalComp->getUuid()) {
                    onInputSlotChange(id, direction, signalKind, newCount);
                } else if (id == outputDigitalComp->getUuid()) {
                    onOutputSlotChange(id, direction, signalKind, newCount);
                }
            });
    }

    void ModuleSceneComponent::onAttach(SceneState &state) {
        SimulationSceneComponent::onAttach(state);
        if (state.runtime().scenes && state.runtime().sim) {
            setCallbacks(state);
        }
    }

    void ModuleSceneComponent::onRuntimeReady(SceneState &state) {
        setCallbacks(state);
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state,
                                                    UUID caller) {
        return SimulationSceneComponent::cleanup(state, caller);
    }

    std::vector<std::shared_ptr<SceneComponent>>
    ModuleSceneComponent::createNew(SceneDriver &scenes,
                                    SimEngine::SimulationEngine &sim,
                                    UUID &moduleInpId,
                                    UUID &moduleOutId) {
        auto newScene = scenes.createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);

        auto moduleDef = SimEngine::ModuleDefinition::createNew(sim);
        if (!moduleDef) {
            scenes.removeScene(newSceneState.getSceneId());
            return {};
        }
        auto comps = SimulationSceneComponent::createNew<ModuleSceneComponent>(
            moduleDef);

        auto moduleComp =
            std::dynamic_pointer_cast<ModuleSceneComponent>(comps.front());
        moduleComp->setSceneId(newSceneState.getSceneId());
        moduleComp->m_transform.position.z =
            scenes.getActiveScene()->getNextZCoord();
        moduleComp->getStyle().headerColor = ViewportTheme::colors.moduleColor;
        newSceneState.setModuleId(moduleComp->getUuid());

        // adding module input
        const auto inpDef = sim.getComponentDefinition(moduleDef->getInputId());
        auto inpComps =
            SimulationSceneComponent::createNew<SimulationSceneComponent>(
                inpDef);
        const auto inpSceneComp =
            std::dynamic_pointer_cast<SimulationSceneComponent>(
                inpComps.front());
        inpSceneComp->setName("Module Input");
        inpSceneComp->getTransform().position.z = newScene->getNextZCoord();
        inpSceneComp->getTransform().position.x = -200.f;
        inpSceneComp->setSimEngineId(moduleDef->getInputId());
        moduleInpId = inpSceneComp->getUuid();
        moduleComp->m_associatedInp = inpSceneComp->getUuid();
        inpComps.erase(inpComps.begin());

        newSceneState.addComponent(inpSceneComp);

        for (const auto &inpComp : inpComps) {
            newSceneState.addComponent(inpComp);
            newSceneState.attachChild(
                inpSceneComp->getUuid(), inpComp->getUuid(), false);
        }

        // adding module output
        auto outDef = sim.getComponentDefinition(moduleDef->getOutputId());
        auto outComps = SimulationSceneComponent::createNew(outDef);
        auto outSceneComp = std::dynamic_pointer_cast<SimulationSceneComponent>(
            outComps.front());
        outSceneComp->setName("Module Output");
        outSceneComp->getTransform().position.z = newScene->getNextZCoord();
        outSceneComp->getTransform().position.x = 200.f;
        outSceneComp->setSimEngineId(moduleDef->getOutputId());
        moduleOutId = outSceneComp->getUuid();
        moduleComp->m_associatedOut = outSceneComp->getUuid();
        outComps.erase(outComps.begin());

        newSceneState.addComponent(outSceneComp);
        for (const auto &outComp : outComps) {
            outComp->setIsSelected(false);
            newSceneState.addComponent(outComp);
            newSceneState.attachChild(
                outSceneComp->getUuid(), outComp->getUuid(), false);
        }

        return comps;
    }

    bool
    ModuleSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button == Canvas::Events::MouseButton::left &&
            e.action == Canvas::Events::MouseClickAction::doubleClick) {
            auto viewportPanel =
                Bess::UI::UIMain::getTargetSceneViewportPanel();
            viewportPanel->updateAttachedSceneId(m_sceneId);
            return true;
        }
        return false;
    }
} // namespace Bess::Canvas
