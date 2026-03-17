#include "module_scene_component.h"
#include "common/bess_uuid.h"
#include "module_def.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/input_scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"

namespace Bess::Canvas {

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

        auto moduleComp = comps.front();
        comps.erase(comps.begin());
        sceneState.addComponent(moduleComp);
        for (const auto &comp : comps) {
            sceneState.addComponent(comp);
            sceneState.attachChild(moduleComp->getUuid(), comp->getUuid());
        }

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
