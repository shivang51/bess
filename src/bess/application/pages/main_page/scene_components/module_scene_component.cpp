#include "module_scene_component.h"
#include "bess_core/copy_paste_service.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene_driver.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "dig_module_def.h"
#include "dig_sim_driver.h"
#include "icons/FontAwesomeIcons.h"
#include "pages/main_page/cmds/module_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "simulation_engine.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

namespace Bess::Canvas {
    ModuleSceneComponent::ModuleSceneComponent() {
        m_icon = UI::Icons::FontAwesomeIcons::FA_CUBES;
    };

    std::vector<std::shared_ptr<SceneComponent>>
    ModuleSceneComponent::clone(const SceneState &sceneState) const {
        auto moduleClone = std::make_shared<ModuleSceneComponent>(*this);
        auto clonedComps = cloneSimulationComponent(sceneState, moduleClone);

        const auto &clonedModDef =
            std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(
                moduleClone->getCompDef());

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();

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
            ogScene->selectAllEntities();
            Svc::CopyPaste::Context cpCtx;
            cpCtx.onInit();
            cpCtx.copy(ogScene);
            ogToCloneId = cpCtx.paste(newScene, false);
            cpCtx.onDestroy();
            ogScene->getState().clearSelectedComponents();
        }

        BESS_ASSERT(ogToCloneId.contains(m_associatedInp),
                    "[CloneModule] Associated input cloned mapping not found");

        BESS_ASSERT(ogToCloneId.contains(m_associatedOut),
                    "[CloneModule] Associated output cloned mapping not found");

        const auto &clonedInpId = ogToCloneId.at(m_associatedInp);
        const auto &clonedOutId = ogToCloneId.at(m_associatedOut);

        moduleClone->setAssociatedInp(clonedInpId);
        moduleClone->setAssociatedOut(clonedOutId);

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();

        // becuase onAttach simulation scene component creates its own dig comp
        // in simulation engine
        auto clonedInp =
            newSceneState.getComponentByUuid<SimulationSceneComponent>(
                clonedInpId);
        BESS_ASSERT(clonedInp, "[CloneModule] Cloned associated input "
                               "component not found in new scene");
        simEngine.deleteComponent(clonedModDef->getInputId());
        clonedModDef->setInputId(clonedInp->getSimEngineId());
        auto clonedOut =
            newSceneState.getComponentByUuid<SimulationSceneComponent>(
                clonedOutId);
        BESS_ASSERT(clonedOut, "[CloneModule] Cloned associated output "
                               "component not found in new scene");
        simEngine.deleteComponent(clonedModDef->getOutputId());
        clonedModDef->setOutputId(clonedOut->getSimEngineId());

        return clonedComps;
    }

    void ModuleSceneComponent::setCallbacks(const SceneState &state) {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();
        auto moduleDef =
            std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(m_compDef);
        BESS_ASSERT(moduleDef, "[ModuleSceneComponent] Module definition not "
                               "found while setting callbacks");
        const auto ownerSceneId = state.getSceneId();

        const auto &outputDigitalComp =
            simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                moduleDef->getOutputId());
        const auto &inputDigitalComp =
            simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                moduleDef->getInputId());
        auto moduleDigComp =
            simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                m_simEngineId);

        BESS_ASSERT(outputDigitalComp,
                    "[ModuleSceneComponent] Missing output sim component {} "
                    "for module {}",
                    (uint64_t)moduleDef->getOutputId(), (uint64_t)m_uuid);

        BESS_ASSERT(inputDigitalComp,
                    "[ModuleSceneComponent] Missing input sim component {} for "
                    "module {}",
                    (uint64_t)moduleDef->getInputId(), (uint64_t)m_uuid);

        BESS_ASSERT(moduleDigComp,
                    "[ModuleSceneComponent] Missing module sim component {} "
                    "for module {}",
                    (uint64_t)m_simEngineId, (uint64_t)m_uuid);

        if (!outputDigitalComp || !inputDigitalComp || !moduleDigComp) {
            BESS_ERROR(
                "[ModuleSceneComponent] Failed to bind callbacks for module "
                "{}. Missing sim components. module={}, input={}, output={}",
                m_name, (uint64_t)m_simEngineId,
                (uint64_t)moduleDef->getInputId(),
                (uint64_t)moduleDef->getOutputId());
            return;
        }

        outputDigitalComp->removeOnStateChangeCB(m_uuid);
        outputDigitalComp->addOnStateChangeCB(
            m_uuid,
            [this](const std::vector<SimEngine::SlotState> &inputStates,
                   const std::vector<SimEngine::SlotState> &outputStates) {
                auto &appCtx = Bess::GAppContext::getInstance();
                auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
                auto &simEngine = projectCtx->getSimEngine();
                auto moduleDigComp =
                    simEngine
                        .getComponent<SimEngine::Drivers::Digital::DigSimComp>(
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
                    simEngine.setOutputSlotState(this->m_simEngineId, 0,
                                                 outputs[0].getLogicState());
                }
            });

        auto onOutputSlotChange = [this, ownerSceneId](const UUID &id,
                                                       SimEngine::SlotType type,
                                                       int newCount) {
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            auto &simEngine = projectCtx->getSimEngine();
            auto moduleDigComp =
                simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                    this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }

            const auto moduleDef =
                moduleDigComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            const auto currCount = moduleDef->getOutputSlotsInfo().count;

            auto sceneDriver = GAppContext::getInstance()
                                   .getSubSystem<Bess::ProjectContext>()
                                   ->getSubSystem<SceneDriver>();
            const auto ownerScene = sceneDriver->getSceneWithId(ownerSceneId);
            if (!ownerScene) {
                return;
            }
            auto &ownerSceneState = ownerScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    simEngine.addSlot(this->m_simEngineId,
                                      SimEngine::SlotType::digitalOutput,
                                      (int)i, true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setSlotType(SlotType::digitalOutput);
                    m_outputSlots.push_back(slot->getUuid());
                    ownerSceneState.addComponent(slot, false, false);
                    ownerSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    simEngine.removeSlot(this->m_simEngineId,
                                         SimEngine::SlotType::digitalOutput,
                                         (int)i, true);
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

        auto onInputSlotChange = [this, ownerSceneId](const UUID &id,
                                                      SimEngine::SlotType type,
                                                      int newCount) {
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            auto &simEngine = projectCtx->getSimEngine();
            auto moduleDigComp =
                simEngine.getComponent<SimEngine::Drivers::Digital::DigSimComp>(
                    this->m_simEngineId);
            if (!moduleDigComp) {
                return;
            }

            const auto moduleDef =
                moduleDigComp
                    ->getDefinition<SimEngine::Drivers::Digital::DigCompDef>();
            const auto currCount = moduleDef->getInputSlotsInfo().count;

            auto sceneDriver = GAppContext::getInstance()
                                   .getSubSystem<Bess::ProjectContext>()
                                   ->getSubSystem<SceneDriver>();
            const auto ownerScene = sceneDriver->getSceneWithId(ownerSceneId);
            if (!ownerScene) {
                return;
            }
            auto &ownerSceneState = ownerScene->getState();

            if (newCount > currCount) {
                for (size_t i = currCount; i < newCount; ++i) {
                    simEngine.addSlot(this->m_simEngineId,
                                      SimEngine::SlotType::digitalInput, (int)i,
                                      true);
                    auto slot = std::make_shared<SlotSceneComponent>();
                    slot->setIndex((int)i);
                    slot->setSlotType(SlotType::digitalInput);
                    m_inputSlots.push_back(slot->getUuid());
                    ownerSceneState.addComponent(slot, false, false);
                    ownerSceneState.attachChild(m_uuid, slot->getUuid(), false);
                }
            } else if (newCount < currCount) {
                for (size_t i = newCount; i < currCount; ++i) {
                    simEngine.removeSlot(this->m_simEngineId,
                                         SimEngine::SlotType::digitalInput,
                                         (int)i, true);
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
        simEngine.removeOnSlotCountChangeCB(m_simEngineId);
        simEngine.addOnSlotCountChangeCB(
            m_simEngineId,
            [inputDigitalComp, outputDigitalComp, onInputSlotChange,
             onOutputSlotChange](const UUID &id, SimEngine::SlotType type,
                                 int newCount) {
                if (id == inputDigitalComp->getUuid()) {
                    onInputSlotChange(id, type, newCount);
                } else if (id == outputDigitalComp->getUuid()) {
                    onOutputSlotChange(id, type, newCount);
                }
            });
    }

    void ModuleSceneComponent::onAttach(SceneState &state) {
        SimulationSceneComponent::onAttach(state);
        setCallbacks(state);
    }

    std::vector<UUID> ModuleSceneComponent::cleanup(SceneState &state,
                                                    UUID caller) {
        return {};
    }

    std::vector<std::shared_ptr<SceneComponent>>
    ModuleSceneComponent::createNew(UUID &moduleInpId, UUID &moduleOutId) {
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();

        auto newScene = sceneDriver->createNewScene();
        auto &newSceneState = newScene->getState();
        newSceneState.setIsRootScene(false);

        auto moduleDef = SimEngine::ModuleDefinition::createNew();
        auto comps = SimulationSceneComponent::createNew<ModuleSceneComponent>(
            moduleDef);

        auto moduleComp =
            std::dynamic_pointer_cast<ModuleSceneComponent>(comps.front());
        moduleComp->setSceneId(newSceneState.getSceneId());
        moduleComp->m_transform.position.z =
            sceneDriver->getActiveScene()->getNextZCoord();
        moduleComp->getStyle().headerColor = ViewportTheme::colors.moduleColor;
        newSceneState.setModuleId(moduleComp->getUuid());

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        const auto &simEngine = projectCtx->getSimEngine();

        // adding module input
        const auto inpDef =
            simEngine.getComponentDefinition(moduleDef->getInputId());
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
            newSceneState.attachChild(inpSceneComp->getUuid(),
                                      inpComp->getUuid(), false);
        }

        // adding module output
        auto outDef =
            simEngine.getComponentDefinition(moduleDef->getOutputId());
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
            newSceneState.attachChild(outSceneComp->getUuid(),
                                      outComp->getUuid(), false);
        }

        return comps;
    }

    std::shared_ptr<ModuleSceneComponent>
    ModuleSceneComponent::fromNet(const UUID &netId, const std::string &name) {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        auto command = std::make_unique<Cmd::CreateModuleCmd>(
            sceneDriver->getActiveScene(), netId, name);
        auto *commandPtr = command.get();
        mainPageState.getCommandSystem().execute(std::move(command));
        return commandPtr->getModuleComponent();
    }

    void
    ModuleSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button == Canvas::Events::MouseButton::left &&
            e.action == Canvas::Events::MouseClickAction::doubleClick) {
            auto sceneDriver = GAppContext::getInstance()
                                   .getSubSystem<Bess::ProjectContext>()
                                   ->getSubSystem<SceneDriver>();
            sceneDriver->setActiveScene(m_sceneId);
        }
    }
} // namespace Bess::Canvas
